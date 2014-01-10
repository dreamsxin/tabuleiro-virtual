#ifndef ENT_TABULEIRO_H
#define ENT_TABULEIRO_H

#include <map>
#include <vector>
#include "ent/entidade.pb.h"
#include "ent/tabuleiro.pb.h"
#include "ntf/notificacao.h"

namespace ntf {
  class Notificacao;
}

namespace ent {

class Entidade;

/** botoes do mouse. */
enum botao_e {
  BOTAO_NENHUM,
  BOTAO_ESQUERDO,
  BOTAO_DIREITO,
  BOTAO_MEIO
};

/** Estados possiveis do tabuleiro. */
enum etab_t {
  ETAB_OCIOSO,
  ETAB_ROTACAO,
  ETAB_DESLIZANDO,
  ETAB_ENT_PRESSIONADA,
  ETAB_ENT_SELECIONADA,
  ETAB_QUAD_PRESSIONADO,
  ETAB_QUAD_SELECIONADO,
};

typedef std::map<unsigned int, Entidade*> MapaEntidades;

/** Responsavel pelo mundo do jogo. O sistema de coordenadas tera X apontando para o leste, 
* Y para o norte e Z para alto. Cada unidade corresponde a um metro, portanto os quadrados 
* sao de tamanho 1,5m.
*/
class Tabuleiro : public ntf::Receptor {
 public:
  explicit Tabuleiro(ntf::CentralNotificacoes* central);

  /** libera os recursos do tabuleiro, inclusive entidades. */
  virtual ~Tabuleiro();

  /** @return tamanho do tabuleiro em X. */
  int TamanhoX() const;

  /** @return tamanho do tabuleiro em Y. */
  int TamanhoY() const;

  /** adiciona a entidade ao tabuleiro, através de uma notificação.
  * @throw logic_error se o limite de entidades for alcançado.
  */
  void AdicionaEntidade(const ntf::Notificacao& notificacao);

  /** remove entidade do tabuleiro, pelo id da entidade passada ou a selecionada. 
  */
  void RemoveEntidade(const ntf::Notificacao& notificacao);

  /** desenha o mundo. */
  void Desenha();

  /** Interface receptor. */
  virtual bool TrataNotificacao(const ntf::Notificacao& notificacao) override;

  /** trata evento de rodela de mouse. */
  void TrataRodela(int delta);

  /** trata movimento do mouse (y ja em coordenadas opengl). */
  void TrataMovimento(botao_e botao, int x, int y);

  /** trata o botao do mouse liberado. */
  void TrataBotaoLiberado();

  /** trata o botao pressionado, recebendo x, y (ja em coordenadas opengl) e
  * a razao de aspecto da janela.
  */
  void TrataBotaoPressionado(botao_e botao, int x, int y, double aspecto);

  /** Trata o click duplo, recebendo x, y (coordenadas opengl) e a razão de aspecto da janela. */
  void TrataDuploClique(botao_e botao, int x, int y, double aspecto);

  /** trata a redimensao da janela. */
  void TrataRedimensionaJanela(int largura, int altura);

  /** inicializa os parametros do openGL. */
  static void InicializaGL();

 private:
  /** funcao que desenha a cena independente do modo.
  */
  void DesenhaCena();

  /** Atualiza a posição do olho na direção do quadrado selecionado ou da entidade selecionada. */
  void AtualizaOlho();

  /** Encontra os hits de um clique em objetos. */
  void EncontraHits(int x, int y, double aspecto, unsigned int* numero_hits, unsigned int* buffer_hits);

  /** trata o clique, recebendo o numero de hits e o buffer de hits do opengl. */
  void TrataClique(unsigned int numero_hits, unsigned int* buffer_hits);

  /** Trata o clique duplo, recebendo o numero de hits e o buffer de hits do opengl. */
  void TrataDuploCliqueEsquerdo(unsigned int numero_hits, unsigned int* buffer_hits);
  /** Trata o duplo clique com botao direito. */
  void TrataDuploCliqueDireito(int x, int y);

  /** seleciona a entidade pelo ID. */ 
  void SelecionaEntidade(unsigned int id);
  void DeselecionaEntidade();

  /** seleciona o quadrado pelo ID. */
  void SelecionaQuadrado(int id_quadrado);

  /** retorna as coordenadas do quadrado. */
  void CoordenadaQuadrado(int id_quadrado, double* x, double* y, double* z);

  /** @return uma notificacao do tipo TN_SERIALIZAR_TABULEIRO preenchida. */
  ntf::Notificacao* SerializaTabuleiro();

  /** @return uma notificacao do tipo TN_ABRIR_DIALOGO_ILUMINACAO preenchida. */
  ntf::Notificacao* SerializaIluminacaoTabuleiro();

  /** Monta o tabuleiro de acordo com a notificacao TN_DESERIALIZAR_TABULEIRO. */
  void DeserializaTabuleiro(const ntf::Notificacao& notificacao);

  /** @return a entidade por id, ou nullptr se nao encontrá-la. */
  Entidade* BuscaEntidade(unsigned int id);

  /** Remove uma entidade pelo id.
  * @return true se a entidade removida for a selecionada. 
  */
  bool RemoveEntidade(unsigned int id);

 private:
  // Parametros de desenho, importante para operacoes de picking e manter estado durante renderizacao.
  ParametrosDesenho parametros_desenho_;
  // Parametros do tabuleiro (sem entidades).
  TabuleiroProto proto_;

  /** Cada cliente possui um identificador diferente. */
  int id_cliente_;

  /** mapa geral de entidades, por id. */
  MapaEntidades entidades_;

  /** a entidade selecionada. */
  Entidade* entidade_selecionada_;

  /** quadrado selecionado (pelo id de desenho). */
  int quadrado_selecionado_;

  /** estado do tabuleiro. */
  etab_t estado_;
  /** usado para restaurar o estado apos rotacao. */
  etab_t estado_anterior_rotacao_;

  /** proximo id local de entidades. */
  int proximo_id_entidade_;

  /** proximo id de cliente. */
  int proximo_id_cliente_;

  /** dados (X) para calculo de mouse. */
  int ultimo_x_; 
  /** dados (Y) para calculo de mouse. */
  int ultimo_y_; 

  // Para onde o olho olha.
  Olho olho_;

  ntf::CentralNotificacoes* central_;

  // elimina copia
  Tabuleiro(const Tabuleiro& t);
  Tabuleiro& operator=(Tabuleiro&);
};

}

#endif
