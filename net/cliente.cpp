#include <algorithm>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/asio.hpp>
#include <vector>
#include <string>

#include "log/log.h"
#include "net/cliente.h"
#include "ntf/notificacao.h"
#include "ntf/notificacao.pb.h"

using boost::asio::ip::tcp;

namespace net {

Cliente::Cliente(boost::asio::io_service* servico_io, ntf::CentralNotificacoes* central) {
  servico_io_ = servico_io;
  central_ = central;
  central_->RegistraReceptor(this);
  // TODO fazer alguma verificacao disso.
  // tamanho maximo da mensagem: 1MB.
  buffer_.resize(1 * 1024 * 1024);
  // Tamanho maximo da notificacao: 10MB.
  buffer_notificacao_.reserve(10 * 1024 * 1024);
  a_receber_ = 0;
}

bool Cliente::TrataNotificacao(const ntf::Notificacao& notificacao) {
  if (notificacao.tipo() == ntf::TN_TEMPORIZADOR) {
    if (Ligado()) {
      servico_io_->poll_one();
    }
    return true;
  } else if (notificacao.tipo() == ntf::TN_CONECTAR) {
    Conecta(notificacao.endereco());
    return true;
  } else if (notificacao.tipo() == ntf::TN_DESCONECTAR) {
    Desconecta();
    return true;
  }
  return false;
}

bool Cliente::TrataNotificacaoRemota(const ntf::Notificacao& notificacao) {
  EnviaDados(notificacao.SerializeAsString());
  return true;
}

// Codifica os dados, colocando tamanho na frente. O tamanho eh codificado de
// forma LSB first.
const std::vector<char> CodificaDados(const std::string& dados) {
  size_t tam_dados = dados.size();
  std::vector<char> ret(4);
  ret[0] = static_cast<char>(tam_dados & 0xFF);
  ret[1] = static_cast<char>((tam_dados & 0xFF00) >> 8);
  ret[2] = static_cast<char>((tam_dados & 0xFF0000) >> 16);
  ret[3] = static_cast<char>((tam_dados & 0xFF000000) >> 24);
  ret.insert(ret.end(), dados.begin(), dados.end());
  return ret;
}

int DecodificaTamanho(const std::vector<char>& buffer) {
  return buffer[0] | (buffer[1] << 8) | (buffer[2] << 16) | (buffer[3] << 24);
}

void Cliente::EnviaDados(const std::string& dados) {
  size_t bytes_enviados = socket_->send(boost::asio::buffer(CodificaDados(dados)));
  if (bytes_enviados != dados.size()) {
    LOG(ERROR) << "Erro enviando dados, enviado: " << bytes_enviados;
  } else {
    LOG(INFO) << "Enviei " << dados.size() << " bytes pro servidor.";
  }
}
void Cliente::Conecta(const std::string& endereco_str) {
  std::vector<std::string> endereco_porta;
  boost::split(endereco_porta, endereco_str, boost::algorithm::is_any_of(":"));
  if (endereco_porta.size() == 0) {
    // Endereco padrao.
    endereco_porta.push_back("localhost");
  } else if (endereco_porta[0].empty()) {
    endereco_porta[0] = "localhost";
  }
  if (endereco_porta.size() == 1) {
    // Porta padrao.
    endereco_porta.push_back("11223");
  }
  try {
    socket_.reset(new boost::asio::ip::tcp::socket(*servico_io_));
    boost::asio::ip::tcp::resolver resolver(*servico_io_);
    auto endereco_resolvido = resolver.resolve({endereco_porta[0], endereco_porta[1]});
    boost::asio::connect(*socket_, endereco_resolvido);
    // Handler de leitura.
    auto* notificacao = new ntf::Notificacao;
    notificacao->set_tipo(ntf::TN_RESPOSTA_CONEXAO);
    central_->AdicionaNotificacao(notificacao);
    central_->RegistraReceptorRemoto(this);
    RecebeDados();
    VLOG(1) << "Conexão bem sucedida";
  } catch (std::exception& e) {
    socket_.reset();
    auto* notificacao = new ntf::Notificacao;
    notificacao->set_tipo(ntf::TN_RESPOSTA_CONEXAO);
    notificacao->set_erro(e.what());
    central_->AdicionaNotificacao(notificacao);
    VLOG(1) << "Falha de conexão";
  }
}

void Cliente::Desconecta() {
  if (!Ligado()) {
    return;
  }
  socket_->close();
  socket_.reset();
  auto* notificacao = new ntf::Notificacao;
  notificacao->set_tipo(ntf::TN_DESCONECTADO);
  central_->AdicionaNotificacao(notificacao);
  central_->DesregistraReceptorRemoto(this);
  std::cout << "Desconectando..." << std::endl;
}

bool Cliente::Ligado() const {
  return socket_ != nullptr;
}

void Cliente::RecebeDados() {
  LOG(INFO) << "Cliente::RecebeDados";
  socket_->async_receive(
    boost::asio::buffer(buffer_),
    [this](boost::system::error_code ec, std::size_t bytes_recebidos) {
      if (ec) {
        LOG(ERROR) << "Erro recebendo dados: " << ec.message();
        Desconecta();
        return;
      }
      auto buffer_it = buffer_.begin();
      do {
        if (a_receber_ == 0) {
          if (bytes_recebidos < 4) {
            LOG(ERROR) << "Erro recebendo tamanho de dados do servidor, bytes recebidos: " << bytes_recebidos;
            Desconecta();
            return;
          }
          a_receber_ = DecodificaTamanho(buffer_);
          buffer_it += 4;
        }
        if (buffer_.end() - buffer_it > a_receber_) {
          // Quantidade de dados recebida eh maior ou igual ao esperado (por exemplo, ao receber duas mensagens juntas).
          buffer_notificacao_.insert(buffer_notificacao_.end(), buffer_it, buffer_it + a_receber_);
          // Decodifica mensagem e poe na central.
          auto* notificacao = new ntf::Notificacao;
          if (!notificacao->ParseFromString(buffer_notificacao_)) {
            LOG(ERROR) << "Erro ParseFromString recebendo dados do servidor.";
            delete notificacao;
            Desconecta();
            return;
          }
          central_->AdicionaNotificacao(notificacao);
          buffer_notificacao_.clear();
          buffer_it += a_receber_;
        } else {
          // Quantidade de dados recebida eh menor que o esperado. Poe no buffer
          // e sai.
          buffer_notificacao_.insert(buffer_notificacao_.end(), buffer_it, buffer_.end());
          buffer_it = buffer_.end();
        }
      } while (buffer_it != buffer_.end());
      RecebeDados();
    }
  );
}

}  // namespace net
