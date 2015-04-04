#ifndef TEX_TEXTURAS_H
#define TEX_TEXTURAS_H

#include <unordered_map>
#include "ent/entidade.h"
#include "ntf/notificacao.h"

#define DIR_TEXTURAS "texturas"

namespace tex {

/** Gerencia carregamento de texturas atraves de notificacoes. */
class Texturas : public ent::Texturas, public ntf::Receptor {
 public:
  Texturas(ntf::CentralNotificacoes* central);
  virtual ~Texturas();

  /** Trata as notificacoes do tipo de carregamento descarregamento de textura. */
  virtual bool TrataNotificacao(const ntf::Notificacao& notificacao) override;

  /** Retorna uma textura. */
  virtual unsigned int Textura(const std::string& id) const override;

  /** Recarrega todas as texturas (em caso de perda do contexto OpenGL, no android por exemplo).
  * @param rele tambem realiza a releitura dos bits crus, decodificando-os.
  */
  void Recarrega(bool rele = false);

  /** Le e decodifica uma imagem. Os bits crus so seram preenchidos se nao for global ou se forcar. */
  static void LeDecodificaImagem(bool global, bool forcar_bits_crus, const std::string& caminho, ent::InfoTextura* info_textura);

 private:
  struct InfoTexturaInterna;

  /** Auxiliar para retornar informacao de textura. @return nullptr se nao houver. */
  InfoTexturaInterna* InfoInterna(const std::string& id);
  const InfoTexturaInterna* InfoInterna(const std::string& id) const;

  /** Realiza o carregamento da textura ou referenciamento de uma textura ou referencia . */
  void CarregaTextura(const ent::InfoTextura& info);

  /** Descarrega ou dereferencia uma textura. */
  void DescarregaTextura(const ent::InfoTextura& info);

  /** Gera um identificador unico de textura.
  * @return -1 se alcancar o limite de texturas.
  */
  int GeraIdTextura();

  /** Realiza a leitura da imagem de um caminho, preenchendo dados com conteudo do arquivo no caminho.
  * Caso local, a textura sera local ao jogador. Caso contrario, eh uma textura global (da aplicacao).
  */
  static void LeImagem(bool global, const std::string& arquivo, std::vector<unsigned char>* dados);

  /** Decodifica os dados_crus, preenchendo info_textura. */
  static void DecodificaImagem(const std::vector<unsigned char>& dados_crus, ent::InfoTextura* info_textura);

 private:
  // Nao possui.
  ntf::CentralNotificacoes* central_;
  // Mapeia id da textura para sua informacao interna.
  std::unordered_map<std::string, InfoTexturaInterna*> texturas_;
};

}  // namespace tex

#endif
