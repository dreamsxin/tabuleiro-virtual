#include <boost/asio.hpp>
#include <memory>
#include <vector>
#include "ntf/notificacao.h"

namespace net {

class Servidor : public ntf::Receptor, public ntf::ReceptorRemoto {
 public:
  // Nao possui os parametros.
  Servidor(boost::asio::io_service* servico_io, ntf::CentralNotificacoes* central);

  virtual bool TrataNotificacao(const ntf::Notificacao& notificacao) override;
  virtual bool TrataNotificacaoRemota(const ntf::Notificacao& notificacao) override;

 private:
  // Liga o servidor e chama EsperaCliente.
  void Liga();
  // Desliga o servidor.
  void Desliga();

  // Retorna se o servidor esta ligado.
  bool Ligado() const;

  // Funcao que espera um cliente de forma assincrona e renova automaticamente sempre que um cliente aparece.
  // Para cada cliente, chama RecebeDadosCliente e Nao bloca.
  void EsperaCliente();
  // Chama a funcao de recepcao de dados de forma assincrona para o cliente.
  void RecebeDadosCliente(boost::asio::ip::tcp::socket* cliente);
  // Envia dados para o cliente.
  void EnviaDadosCliente(boost::asio::ip::tcp::socket* cliente, const std::string& dados);

  ntf::CentralNotificacoes* central_;
  boost::asio::io_service* servico_io_;
  std::unique_ptr<boost::asio::ip::tcp::acceptor> aceitador_;
  std::unique_ptr<boost::asio::ip::tcp::socket> cliente_;
  std::vector<boost::asio::ip::tcp::socket*> clientes_pendentes_;
  std::vector<boost::asio::ip::tcp::socket*> clientes_;
  std::vector<char> buffer_;
};

}  // namespace net
