#include <fstream>
#include <stdexcept>
#include "ent/entidade.h"
#include "ent/entidade.pb.h"
#include "gltab/gl.h"
#include "log/log.h"
#include "ntf/notificacao.pb.h"
#include "tex/lodepng.h"
#include "tex/texturas.h"

namespace tex {

namespace {

/** Retorna o formato OpenGL de uma imagem, por exemplo: GL_BGRA. */
int FormatoImagem() {
  return GL_RGBA;
}

/** Retorna o tipo OpenGL de uma imagem, por exemplo: GL_UNSIGNED_INT_8_8_8_8_REV. */
int TipoImagem() {
  return GL_UNSIGNED_BYTE;
}

}  // namespace

struct Texturas::InfoTexturaInterna {
  InfoTexturaInterna(const std::string& id_mapa, const ent::InfoTextura& imagem) : contador(1) {
    imagem_ = imagem;
    try {
      CriaTexturaOpenGl();
    } catch (...) {
      imagem_.Clear();
      LOG(ERROR) << "Erro gerando nome para textura";
      return;
    }
    VLOG(1) << "Textura criada: id: '" << id_mapa << "', id OpenGL: '" << id
            << "', " << imagem.largura() << "x" << imagem.altura()
            << ", format: " << FormatoImagem();
  }

  ~InfoTexturaInterna() {
    if (id == GL_INVALID_VALUE) {
      return;
    }
    GLuint tex_name = id;
    glDeleteTextures(1, &tex_name);
  }

  void CriaTexturaOpenGl() {
    glGenTextures(1, &id);
    if (id == GL_INVALID_VALUE) {
      throw std::logic_error("Erro criando textura (glGenTextures)");
    }
    glBindTexture(GL_TEXTURE_2D, id);
    // Mapeamento de texels em amostragem para cima e para baixo (mip maps).
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    // Carrega a textura.
    glTexImage2D(GL_TEXTURE_2D,
                 0, GL_RGBA,
                 imagem_.altura(), imagem_.largura(),
                 0, FormatoImagem(), TipoImagem(),
                 imagem_.bits().c_str());
    glDisable(GL_TEXTURE_2D);
  }

  ent::InfoTextura imagem_;
  int contador;
  GLuint id;
};

Texturas::Texturas(ntf::CentralNotificacoes* central) {
  central_ = central;
  central_->RegistraReceptor(this);
}

Texturas::~Texturas() {}

// Interface de ent::Texturas.
bool Texturas::TrataNotificacao(const ntf::Notificacao& notificacao) {
  switch (notificacao.tipo()) {
    case ntf::TN_CARREGAR_TEXTURA:
      CarregaTextura(notificacao.info_textura());
      return true;
    case ntf::TN_DESCARREGAR_TEXTURA:
      DescarregaTextura(notificacao.info_textura());
      return true;
    default: ;
  }
  return false;
}

unsigned int Texturas::Textura(const std::string& id) const {
  const InfoTexturaInterna* info_interna = InfoInterna(id);
  if (info_interna == nullptr) {
    return GL_INVALID_VALUE;
  }
  return info_interna->id;
}
// Fim da interface ent::Texturas.

void Texturas::Recarrega() {
  for (auto& cv : texturas_) {
    cv.second->CriaTexturaOpenGl();
  }
}

Texturas::InfoTexturaInterna* Texturas::InfoInterna(const std::string& id) {
  auto it = texturas_.find(id);
  if (it == texturas_.end()) {
    return nullptr;
  }
  return it->second;
}

const Texturas::InfoTexturaInterna* Texturas::InfoInterna(const std::string& id) const {
  auto it = texturas_.find(id);
  if (it == texturas_.end()) {
    return nullptr;
  }
  return it->second;
}

void Texturas::CarregaTextura(const ent::InfoTextura& info_textura) {
  auto* info_interna = InfoInterna(info_textura.id());
  if (info_interna == nullptr) {
    if (info_textura.has_bits()) {
      VLOG(1) << "Carregando textura com bits.";
      texturas_.insert(make_pair(info_textura.id(), new InfoTexturaInterna(info_textura.id(), info_textura)));
    } else {
      VLOG(1) << "Carregando textura comum.";
      ent::InfoTextura info_lido;
      try {
        std::string arquivo(DIR_TEXTURAS);
        arquivo += "/" + info_textura.id();
        LeDecodificaImagem(arquivo, &info_lido);
      } catch (const std::exception& e) {
        LOG(ERROR) << "Textura inválida: " << info_textura.ShortDebugString() << ", excecao: " << e.what();
        return;
      }
      if (FormatoImagem() == -1) {
        return;
      }
      texturas_.insert(make_pair(info_textura.id(), new InfoTexturaInterna(info_textura.id(), info_lido)));
    }
  } else {
    ++info_interna->contador;
    VLOG(1) << "Textura '" << info_textura.id() << "' incrementada para " << info_interna->contador;
  }
}

void Texturas::DescarregaTextura(const ent::InfoTextura& info_textura) {
  auto* info_interna = InfoInterna(info_textura.id());
  if (info_interna == nullptr) {
    LOG(WARNING) << "Textura nao existente: " << info_textura.id();
  } else {
    if (--info_interna->contador == 0) {
      VLOG(1) << "Textura liberada: " << info_textura.id();
      delete info_interna;
      texturas_.erase(info_textura.id());
    } else {
      VLOG(1) << "Textura '" << info_textura.id() << "' decrementada para " << info_interna->contador;
    }
  }
}

void Texturas::LeImagem(const std::string& caminho, std::vector<unsigned char>* dados) {
  std::ifstream arquivo(caminho, std::ifstream::binary);
  if (!arquivo) {
    throw std::logic_error(std::string("Caminho invalido de imagem: ") + caminho);
  }
  arquivo.seekg(0, arquivo.end);
  size_t tam = arquivo.tellg();
  arquivo.seekg(0, arquivo.beg);
  dados->resize(tam);
  arquivo.read((char*)dados->data(), tam);
  arquivo.close();
}

void Texturas::LeDecodificaImagem(const std::string& caminho, ent::InfoTextura* info_textura) {
  std::vector<unsigned char> dados_arquivo;
  LeImagem(caminho, &dados_arquivo);
  if (dados_arquivo.size() <= 0) {
    throw std::logic_error(std::string("Erro lendo imagem: ") + caminho);
  }

  //decode
  unsigned int largura, altura;
  std::vector<unsigned char> dados;
  unsigned int error = lodepng::decode(dados, largura, altura, dados_arquivo);
  if (error != 0) {
    throw std::logic_error(std::string("Erro decodificando: ") + lodepng_error_text(error));
  }
  info_textura->mutable_bits()->append(dados.begin(), dados.end());
  info_textura->set_largura(largura);
  info_textura->set_altura(altura);
}

}  // namespace tex
