#include <cmath>
#include <stdexcept>
#include <string>

#include <QBoxLayout>
#include <QColorDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QMessageBox>
#include <QMouseEvent>
#include <QString>

#include "ent/tabuleiro.h"
#include "gltab/gl.h"
#include "ifg/qt/constantes.h"
#include "ifg/qt/ui/entidade.h"
#include "ifg/qt/texturas.h"
#include "ifg/qt/util.h"
#include "ifg/qt/ui/forma.h"
#include "ifg/qt/ui/iluminacao.h"
#include "ifg/qt/ui/opcoes.h"
#include "ifg/qt/visualizador3d.h"
#include "log/log.h"
#include "ntf/notificacao.pb.h"

using namespace std;

namespace ifg {
namespace qt {
namespace {

// Temporizadores * 10ms.
const int MAX_TEMPORIZADOR_TECLADO = 300;
const int MAX_TEMPORIZADOR_MOUSE = 100;

// Retorna uma string de estilo para background-color baseada na cor passada.
const QString CorParaEstilo(const QColor& cor) {
  QString estilo_fmt("background-color: rgb(%1, %2, %3);");
  QString estilo = estilo_fmt.arg(cor.red()).arg(cor.green()).arg(cor.blue());
  VLOG(3) << "Retornando estilo: " << estilo.toStdString();
  return estilo;
}

const QString CorParaEstilo(const ent::Cor& cor) {
  return CorParaEstilo(ProtoParaCor(cor));
}

// Converte um tamanho em string.
const QString TamanhoParaTexto(int tamanho) {
  string str;
  switch (tamanho) {
    case ent::TM_MINUSCULO: return QObject::tr("(minúsculo)");
    case ent::TM_DIMINUTO: return QObject::tr("(diminuto)");
    case ent::TM_MIUDO: return QObject::tr("(miudo)");
    case ent::TM_PEQUENO: return QObject::tr("(pequeno)");
    case ent::TM_MEDIO: return QObject::tr("(médio)");
    case ent::TM_GRANDE: return QObject::tr("(grande)");
    case ent::TM_ENORME: return QObject::tr("(enorme)");
    case ent::TM_IMENSO: return QObject::tr("(imenso)");
    case ent::TM_COLOSSAL: return QObject::tr("(colossal)");
  }
  LOG(ERROR) << "Tamanho inválido: " << tamanho;
  return QObject::tr("desconhecido");
}

// Carrega os dados de uma textura local pro proto 'info_textura'.
bool PreencheProtoTextura(const QFileInfo& info_arquivo, ent::InfoTextura* info_textura) {
  QImageReader leitor_imagem(info_arquivo.absoluteFilePath());
  QImage imagem = leitor_imagem.read();
  if (imagem.isNull()) {
    LOG(ERROR) << "Textura inválida: " << info_textura->id();
    return false;
  }
  info_textura->set_altura(imagem.height());
  info_textura->set_largura(imagem.width());
  info_textura->set_bits(imagem.constBits(), imagem.byteCount());
  info_textura->set_formato(imagem.format());
  return true;
}

// Retorna o caminho para o id de textura.
const QFileInfo IdTexturaParaCaminhoArquivo(const std::string& id) {
  // Encontra o caminho para o arquivo.
  auto pos = id.find("0:");  // pode assumir id zero, ja que so o mestre pode criar.
  QFileInfo fileinfo;
  if (pos == std::string::npos) {
    // Caminho relativo ao DIR_TEXTURAS
    fileinfo.setFile(QString::fromStdString(DIR_TEXTURAS), QString::fromStdString(id));
  } else {
    fileinfo.setFile(QString::fromStdString(DIR_TEXTURAS_LOCAIS), QString::fromStdString(id.substr(pos)));
  }
  LOG(INFO) << "Caminho para texturas: " << fileinfo.fileName().toStdString();
  return fileinfo;
}

// Calcula o dano de uma sequencia de caracteres.
int CalculaDanoSimples(const std::vector<int>::const_iterator& inicio_teclas_normal,
                       const std::vector<int>::const_iterator& fim_teclas_normal) {
  std::vector<int>::const_reverse_iterator inicio_teclas(fim_teclas_normal);
  std::vector<int>::const_reverse_iterator fim_teclas(inicio_teclas_normal);

  int delta = 0;
  int multiplicador = 1;
  for (auto it = inicio_teclas; it < fim_teclas; ++it) {
    if (*it < Qt::Key_0 || *it > Qt::Key_9) {
      LOG(WARNING) << "Tecla inválida para delta pontos de vida";
      continue;
    }
    delta += (*it - Qt::Key_0) * multiplicador;
    multiplicador *= 10;
  }
  VLOG(1) << "Tratando acao de delta pontos de vida, total: " << delta;
  return delta;
}

// Calcula o dano acumulado no vetor de teclas.
const std::vector<int> CalculaDano(const std::vector<int>::const_iterator& inicio_teclas,
                                   const std::vector<int>::const_iterator& fim_teclas) {
  std::vector<int> result;
  auto it_inicio = inicio_teclas;
  for (auto it = inicio_teclas; it < fim_teclas; ++it) {
    if (*it == Qt::Key_Space) {
      result.push_back(CalculaDanoSimples(it_inicio, it));
      it_inicio = it + 1;  // pula o espaco.
    }
  }
  result.push_back(CalculaDanoSimples(it_inicio, fim_teclas));
  return result;
}

}  // namespace

Visualizador3d::Visualizador3d(
    ntf::CentralNotificacoes* central, ent::Tabuleiro* tabuleiro, QWidget* pai)
    :  QGLWidget(QGLFormat(QGL::DepthBuffer | QGL::Rgba | QGL::DoubleBuffer), pai),
       central_(central), tabuleiro_(tabuleiro) {
  temporizador_mouse_ = 0;
  temporizador_teclado_ = 0;
  central_->RegistraReceptor(this);
  setFocusPolicy(Qt::StrongFocus);
  setMouseTracking(true);
  MudaEstado(ESTADO_TEMPORIZANDO_MOUSE);
}

Visualizador3d::~Visualizador3d() {
}

// reimplementacoes
void Visualizador3d::initializeGL() {
  ent::Tabuleiro::InicializaGL();
}

void Visualizador3d::resizeGL(int width, int height) {
  tabuleiro_->TrataRedimensionaJanela(width, height);
}

void Visualizador3d::paintGL() {
  tabuleiro_->Desenha();
}

// notificacao
bool Visualizador3d::TrataNotificacao(const ntf::Notificacao& notificacao) {
  switch (notificacao.tipo()) {
    case ntf::TN_TEMPORIZADOR:
      if (estado_ == ESTADO_TEMPORIZANDO_MOUSE && underMouse()) {
        if (--temporizador_mouse_ == 0) {
          TrataAcaoTemporizadaMouse();
        }
        break;
      } else if (estado_ == ESTADO_TEMPORIZANDO_TECLADO) {
        if (--temporizador_teclado_ == 0) {
          TrataAcaoTemporizadaTeclado();
          MudaEstado(ESTADO_TEMPORIZANDO_MOUSE);
        }
        break;
      }
      break;
    case ntf::TN_INICIADO:
      // chama o resize pra iniciar a geometria e desenha a janela
      resizeGL(width(), height());
      glDraw();
      break;
    case ntf::TN_ABRIR_DIALOGO_ENTIDADE: {
      if (!notificacao.has_entidade()) {
        return false;
      }
      auto* entidade = AbreDialogoEntidade(notificacao);
      if (entidade == nullptr) {
        VLOG(1) << "Alterações descartadas";
        break;
      }
      auto* n = ntf::NovaNotificacao(ntf::TN_ATUALIZAR_ENTIDADE);
      n->mutable_entidade()->Swap(entidade);
      central_->AdicionaNotificacao(n);
      break;
    }
    case ntf::TN_ABRIR_DIALOGO_PROPRIEDADES_TABULEIRO: {
      if (!notificacao.has_tabuleiro()) {
        // O tabuleiro criara a mensagem completa.
        return false;
      }
      auto* tabuleiro = AbreDialogoTabuleiro(notificacao);
      if (tabuleiro == nullptr) {
        VLOG(1) << "Alterações de iluminação descartadas";
        break;
      }
      auto* n = ntf::NovaNotificacao(ntf::TN_ATUALIZAR_TABULEIRO);
      n->mutable_tabuleiro()->Swap(tabuleiro);
      central_->AdicionaNotificacao(n);
      break;
    }
    case ntf::TN_ABRIR_DIALOGO_OPCOES: {
      if (!notificacao.has_opcoes()) {
        // O tabuleiro criara a mensagem completa.
        return false;
      }
      auto* opcoes = AbreDialogoOpcoes(notificacao);
      if (opcoes == nullptr) {
        VLOG(1) << "Alterações de opcoes descartadas";
        break;
      }
      auto* n = ntf::NovaNotificacao(ntf::TN_ATUALIZAR_OPCOES);
      n->mutable_opcoes()->Swap(opcoes);
      central_->AdicionaNotificacao(n);
      break;
    }
    case ntf::TN_ERRO: {
      QMessageBox::warning(this, tr("Erro"), tr(notificacao.erro().c_str()));
      break;
    }
    default: ;
  }
  // Sempre redesenha para evitar qualquer problema de atualizacao.
  glDraw();
  return true;
}

// teclado.
void Visualizador3d::keyPressEvent(QKeyEvent* event) {
  if (estado_ == ESTADO_TEMPORIZANDO_TECLADO) {
    switch (event->key()) {
      case Qt::Key_Escape:
        break;
      case Qt::Key_Enter:
      case Qt::Key_Return:
        // Finaliza temporizacao.
        TrataAcaoTemporizadaTeclado();
        break;
      case Qt::Key_Backspace:
      case Qt::Key_Delete:
        // Finaliza temporizacao.
        teclas_.push_back(event->key());
        TrataAcaoTemporizadaTeclado();
        break;
      default:
        // Nao muda estado mas reinicia o timer.
        teclas_.push_back(event->key());
        temporizador_teclado_ = MAX_TEMPORIZADOR_TECLADO;
        return;
    }
    // Ao terminar, volta pro mouse.
    MudaEstado(ESTADO_TEMPORIZANDO_MOUSE);
    return;
  }
  switch (event->key()) {
    case Qt::Key_Delete:
      central_->AdicionaNotificacao(ntf::NovaNotificacao(ntf::TN_REMOVER_ENTIDADE));
      return;
    case Qt::Key_Up:
      tabuleiro_->TrataMovimentoEntidadesSelecionadas(true, 1);
      return;
    case Qt::Key_Down:
      tabuleiro_->TrataMovimentoEntidadesSelecionadas(true, -1);
      return;
    case Qt::Key_Left:
      tabuleiro_->TrataMovimentoEntidadesSelecionadas(false, -1);
      return;
    case Qt::Key_Right:
      tabuleiro_->TrataMovimentoEntidadesSelecionadas(false, 1);
      return;
    case Qt::Key_G:
      if (event->modifiers() == Qt::ControlModifier) {
        tabuleiro_->AgrupaEntidadesSelecionadas();
      }
      return;
    case Qt::Key_V:
      if (event->modifiers() == Qt::ControlModifier) {
        tabuleiro_->ColaEntidadesSelecionadas();
      } else {
        tabuleiro_->AtualizaBitsEntidadeNotificando(ent::Tabuleiro::BIT_VISIBILIDADE);
      }
      return;
    case Qt::Key_I:
      tabuleiro_->TrataBotaoAlternarIluminacaoMestre();
      return;
    case Qt::Key_L:
      tabuleiro_->AtualizaBitsEntidadeNotificando(ent::Tabuleiro::BIT_ILUMINACAO);
      return;
    case Qt::Key_Y:
      if (event->modifiers() == Qt::ControlModifier) {
        tabuleiro_->TrataComandoRefazer();
        return;
      }
      return;
    case Qt::Key_Z:
      if (event->modifiers() == Qt::ControlModifier) {
        tabuleiro_->TrataComandoDesfazer();
        return;
      }
      tabuleiro_->AtualizaBitsEntidadeNotificando(ent::Tabuleiro::BIT_VOO);
      return;
    case Qt::Key_Q:
      tabuleiro_->AtualizaBitsEntidadeNotificando(ent::Tabuleiro::BIT_CAIDA);
      return;
    case Qt::Key_A:
      MudaEstado(ESTADO_TEMPORIZANDO_TECLADO);
      teclas_.push_back(event->key());
      return;
    case Qt::Key_C:
      if (event->modifiers() == Qt::ControlModifier) {
        tabuleiro_->CopiaEntidadesSelecionadas();
      } else {
        MudaEstado(ESTADO_TEMPORIZANDO_TECLADO);
        teclas_.push_back(event->key());
        return;
      }
      break;
    case Qt::Key_D:
      // Entra em modo de temporizacao.
      MudaEstado(ESTADO_TEMPORIZANDO_TECLADO);
      teclas_.push_back(event->key());
      return;
    case Qt::Key_S:
      tabuleiro_->AtualizaBitsEntidadeNotificando(ent::Tabuleiro::BIT_SELECIONAVEL);
      return;
    //case Qt::Key_K:
      // TEMP TEST.
    //  while (1) ;
    default:
      event->ignore();
  }
}

// mouse

void Visualizador3d::mousePressEvent(QMouseEvent* event) {
  MudaEstado(ESTADO_OUTRO);
  if (event->modifiers() == Qt::AltModifier) {
    // Acao padrao eh usada quando o botao eh o direito.
    tabuleiro_->TrataBotaoAcaoPressionado(
        event->button() == Qt::RightButton, event->x(), height() - event->y());
  } else if (event->modifiers() == Qt::ControlModifier) {
    if (event->button() == Qt::LeftButton) {
      tabuleiro_->TrataBotaoAlternarSelecaoEntidadePressionado(event->x(), height() - event->y());
    } else if (event->button() == Qt::RightButton) {
      tabuleiro_->TrataBotaoDesenhoPressionado(event->x(), height() - event->y());
    }
  } else {
    switch (event->button()) {
      case Qt::LeftButton:
        if (event->modifiers() == Qt::ShiftModifier) {
          // Mac nao tem botao do meio, entao usa o shift para simular.
          tabuleiro_->TrataBotaoRotacaoPressionado(event->x(), height() - event->y());
        } else {
          tabuleiro_->TrataBotaoEsquerdoPressionado(event->x(), height() - event->y());
        }
        break;
      case Qt::RightButton:
        tabuleiro_->TrataBotaoDireitoPressionado(event->x(), height() - event->y());
        break;
      case Qt::MiddleButton:
        tabuleiro_->TrataBotaoRotacaoPressionado(event->x(), height() - event->y());
        break;
      default:
        ;
    }
  }
  glDraw();
  event->accept();
}

void Visualizador3d::mouseReleaseEvent(QMouseEvent* event) {
  MudaEstado(ESTADO_TEMPORIZANDO_MOUSE);
  tabuleiro_->TrataBotaoLiberado();
  event->accept();
  glDraw();
}

void Visualizador3d::mouseDoubleClickEvent(QMouseEvent* event) {
  if (event->modifiers() != 0) {
    // Com modificadores chama o mouse press duas vezes.
    auto* event2 = new QMouseEvent(*event);
    mousePressEvent(event);
    mousePressEvent(event2);
    return;
  }
  if (event->button() == Qt::LeftButton) {
    tabuleiro_->TrataDuploCliqueEsquerdo(event->x(), height() - event->y());
  } else if (event->button() == Qt::RightButton) {
    tabuleiro_->TrataDuploCliqueDireito(event->x(), height() - event->y());
  }
  event->accept();
}

void Visualizador3d::mouseMoveEvent(QMouseEvent* event) {
  ultimo_x_ = event->x();
  ultimo_y_ = height() - event->y();
  if (estado_ == ESTADO_TEMPORIZANDO_MOUSE) {
    temporizador_mouse_ = MAX_TEMPORIZADOR_MOUSE;
    event->accept();
    tabuleiro_->TrataMovimentoMouse();
    return;
  }
  temporizador_mouse_ = MAX_TEMPORIZADOR_MOUSE;
  tabuleiro_->TrataMovimentoMouse(event->x(), (height() - event->y()));
  event->accept();
  glDraw();
}

void Visualizador3d::wheelEvent(QWheelEvent* event) {
  tabuleiro_->TrataEscalaPorDelta(event->delta());
  event->accept();
  glDraw();
}

ent::EntidadeProto* Visualizador3d::AbreDialogoTipoForma(
    const ntf::Notificacao& notificacao) {
  const auto& entidade = notificacao.entidade();
  auto* proto_retornado = new ent::EntidadeProto(entidade);
  ifg::qt::Ui::DialogoForma gerador;
  auto* dialogo = new QDialog(this);
  gerador.setupUi(dialogo);
  // ID.
  QString id_str;
  gerador.campo_id->setText(id_str.setNum(entidade.id()));
  // Pontos de vida.
  gerador.spin_max_pontos_vida->setValue(entidade.max_pontos_vida());
  gerador.spin_pontos_vida->setValue(entidade.pontos_vida());
  // Visibilidade.
  gerador.checkbox_visibilidade->setCheckState(entidade.visivel() ? Qt::Checked : Qt::Unchecked);
  if (!notificacao.modo_mestre()) {
    gerador.checkbox_visibilidade->setEnabled(false);
  }
  // Selecionavel para jogadores.
  gerador.checkbox_selecionavel->setCheckState(entidade.selecionavel_para_jogador() ? Qt::Checked : Qt::Unchecked);
  if (!notificacao.modo_mestre()) {
    gerador.checkbox_selecionavel->setEnabled(false);
  }
  // Cor da entidade.
  ent::EntidadeProto ent_cor;
  ent_cor.mutable_cor()->CopyFrom(entidade.cor());
  gerador.botao_cor->setStyleSheet(CorParaEstilo(entidade.cor()));
  lambda_connect(gerador.botao_cor, SIGNAL(clicked()), [this, dialogo, &gerador, &ent_cor] {
    QColor cor = QColorDialog::getColor(ProtoParaCor(ent_cor.cor()), dialogo, QObject::tr("Cor do objeto"));
    if (!cor.isValid()) {
      return;
    }
    gerador.botao_cor->setStyleSheet(CorParaEstilo(cor));
    ent_cor.mutable_cor()->CopyFrom(CorParaProto(cor));
  });
  gerador.slider_alfa->setValue(static_cast<int>(ent_cor.cor().a() * 100.0f));
  // Rotacao em Z.
  gerador.dial_rotacao->setSliderPosition(entidade.rotacao_z_graus());
  // Translacao em Z.
  gerador.spin_translacao->setValue(entidade.translacao_z());
  // Escalas.
  gerador.spin_escala_x->setValue(entidade.escala().x());
  gerador.spin_escala_y->setValue(entidade.escala().y());
  gerador.spin_escala_z->setValue(entidade.escala().z());

  // Ao aceitar o diálogo, aplica as mudancas.
  lambda_connect(dialogo, SIGNAL(accepted()),
                 [this, notificacao, entidade, dialogo, &gerador, &proto_retornado, &ent_cor ] () {
    if (gerador.spin_max_pontos_vida->value() > 0) {
      proto_retornado->set_max_pontos_vida(gerador.spin_max_pontos_vida->value());
      proto_retornado->set_pontos_vida(gerador.spin_pontos_vida->value());
    } else {
      proto_retornado->clear_max_pontos_vida();
      proto_retornado->clear_pontos_vida();
    }
    proto_retornado->mutable_cor()->Swap(ent_cor.mutable_cor());
    proto_retornado->mutable_cor()->set_a(gerador.slider_alfa->value() / 100.0f);
    proto_retornado->set_visivel(gerador.checkbox_visibilidade->checkState() == Qt::Checked);
    proto_retornado->set_selecionavel_para_jogador(gerador.checkbox_selecionavel->checkState() == Qt::Checked);
    proto_retornado->set_rotacao_z_graus(gerador.dial_rotacao->sliderPosition());
    proto_retornado->set_translacao_z(gerador.spin_translacao->value());
    proto_retornado->mutable_escala()->set_x(gerador.spin_escala_x->value());
    proto_retornado->mutable_escala()->set_y(gerador.spin_escala_y->value());
    proto_retornado->mutable_escala()->set_z(gerador.spin_escala_z->value());
  });
  // TODO: Ao aplicar as mudanças refresca e nao fecha.

  // Cancelar.
  lambda_connect(dialogo, SIGNAL(rejected()), [&notificacao, &proto_retornado] {
      delete proto_retornado;
      proto_retornado = nullptr;
  });
  dialogo->exec();
  delete dialogo;
  return proto_retornado;
}

ent::EntidadeProto* Visualizador3d::AbreDialogoTipoEntidade(
    const ntf::Notificacao& notificacao) {
  const auto& entidade = notificacao.entidade();
  auto* proto_retornado = new ent::EntidadeProto;
  proto_retornado->set_id(entidade.id());
  ifg::qt::Ui::DialogoEntidade gerador;
  auto* dialogo = new QDialog(this);
  gerador.setupUi(dialogo);
  // ID.
  QString id_str;
  gerador.campo_id->setText(id_str.setNum(entidade.id()));
  // Visibilidade.
  gerador.checkbox_visibilidade->setCheckState(entidade.visivel() ? Qt::Checked : Qt::Unchecked);
  if (!notificacao.modo_mestre()) {
    gerador.checkbox_visibilidade->setEnabled(false);
  }
  // Selecionavel para jogadores.
  gerador.checkbox_selecionavel->setCheckState(entidade.selecionavel_para_jogador() ? Qt::Checked : Qt::Unchecked);
  if (!notificacao.modo_mestre()) {
    gerador.checkbox_selecionavel->setEnabled(false);
  }
  // Tamanho.
  gerador.slider_tamanho->setSliderPosition(entidade.tamanho());
  gerador.label_tamanho->setText(TamanhoParaTexto(gerador.slider_tamanho->sliderPosition()));
  lambda_connect(gerador.slider_tamanho, SIGNAL(valueChanged(int)), [&gerador] () {
    gerador.label_tamanho->setText(TamanhoParaTexto(gerador.slider_tamanho->sliderPosition()));
  });
  // Cor da entidade.
  ent::EntidadeProto ent_cor;
  ent_cor.mutable_cor()->CopyFrom(entidade.cor());
  gerador.botao_cor->setStyleSheet(CorParaEstilo(entidade.cor()));
  lambda_connect(gerador.botao_cor, SIGNAL(clicked()), [this, dialogo, &gerador, &ent_cor] {
    QColor cor = QColorDialog::getColor(ProtoParaCor(ent_cor.cor()), dialogo, QObject::tr("Cor do objeto"));
    if (!cor.isValid()) {
      return;
    }
    gerador.botao_cor->setStyleSheet(CorParaEstilo(cor));
    ent_cor.mutable_cor()->CopyFrom(CorParaProto(cor));
  });
  // Cor da luz.
  ent::EntidadeProto luz_cor;
  if (entidade.has_luz()) {
    luz_cor.mutable_cor()->CopyFrom(entidade.luz().cor());
    gerador.botao_luz->setStyleSheet(CorParaEstilo(entidade.luz().cor()));
  } else {
    ent::Cor branco;
    branco.set_r(1.0f);
    branco.set_g(1.0f);
    branco.set_b(1.0f);
    luz_cor.mutable_cor()->CopyFrom(branco);
    gerador.botao_luz->setStyleSheet(CorParaEstilo(branco));
  }
  gerador.checkbox_luz->setCheckState(entidade.has_luz() ? Qt::Checked : Qt::Unchecked);
  lambda_connect(gerador.botao_luz, SIGNAL(clicked()), [this, dialogo, &gerador, &luz_cor] {
    QColor cor =
        QColorDialog::getColor(ProtoParaCor(luz_cor.cor()), dialogo, QObject::tr("Cor da luz"));
    if (!cor.isValid()) {
      return;
    }
    luz_cor.mutable_cor()->CopyFrom(CorParaProto(cor));
    gerador.botao_luz->setStyleSheet(CorParaEstilo(cor));
    gerador.checkbox_luz->setCheckState(Qt::Checked);
  });
  // Textura do objeto.
  gerador.linha_textura->setText(entidade.info_textura().id().c_str());
  lambda_connect(gerador.botao_textura, SIGNAL(clicked()),
      [this, dialogo, &gerador, &luz_cor ] () {
    QString file_str = QFileDialog::getOpenFileName(this, tr("Abrir textura"), tr(DIR_TEXTURAS, FILTRO_IMAGENS));
    if (file_str.isEmpty()) {
      VLOG(1) << "Operação de leitura de textura cancelada.";
      return;
    }
    gerador.linha_textura->setText(file_str);
  });
  // Pontos de vida.
  gerador.spin_pontos_vida->setValue(entidade.pontos_vida());
  gerador.spin_max_pontos_vida->setValue(entidade.max_pontos_vida());
  // Aura.
  gerador.spin_aura->setValue(entidade.aura());
  // Voo.
  gerador.checkbox_voadora->setCheckState(entidade.voadora() ? Qt::Checked : Qt::Unchecked);
  // Caida.
  gerador.checkbox_caida->setCheckState(entidade.caida() ? Qt::Checked : Qt::Unchecked);
  // Morta.
  gerador.checkbox_morta->setCheckState(entidade.morta() ? Qt::Checked : Qt::Unchecked);
  // Translacao em Z.
  gerador.spin_translacao->setValue(entidade.translacao_z());
  // Ao aceitar o diálogo, aplica as mudancas.
  lambda_connect(dialogo, SIGNAL(accepted()),
                 [this, notificacao, entidade, dialogo, &gerador, &proto_retornado, &ent_cor, &luz_cor] () {
    proto_retornado->set_tamanho(static_cast<ent::TamanhoEntidade>(gerador.slider_tamanho->sliderPosition()));
    proto_retornado->mutable_cor()->Swap(ent_cor.mutable_cor());
    if (gerador.checkbox_luz->checkState() == Qt::Checked) {
      proto_retornado->mutable_luz()->mutable_cor()->Swap(luz_cor.mutable_cor());
    } else {
      proto_retornado->clear_luz();
    }
    if (!gerador.linha_textura->text().isEmpty()) {
      if (gerador.linha_textura->text().toStdString() == entidade.info_textura().id()) {
        // Textura igual a anterior.
        VLOG(2) << "Textura igual a anterior.";
        proto_retornado->mutable_info_textura()->set_id(entidade.info_textura().id());
      } else {
        VLOG(2) << "Textura diferente da anterior.";
        QFileInfo info(gerador.linha_textura->text());
        // TODO fazer uma comparacao melhor. Se o diretorio local terminar com o
        // mesmo nome isso vai falhar.
        if (info.dir().dirName() != DIR_TEXTURAS) {
          VLOG(2) << "Textura local, recarregando.";
          QString id = QString::number(notificacao.tabuleiro().id_cliente());
          id.append(":");
          id.append(info.fileName());
          proto_retornado->mutable_info_textura()->set_id(id.toStdString());
          // Usa o id para evitar conflito de textura local com texturas globais.
          // Enviar a textura toda.
          PreencheProtoTextura(info, proto_retornado->mutable_info_textura());
        } else {
          proto_retornado->mutable_info_textura()->set_id(info.fileName().toStdString());
        }
      }
      VLOG(2) << "Id textura: " << proto_retornado->info_textura().id();
    } else {
      proto_retornado->clear_info_textura();
    }
    proto_retornado->set_pontos_vida(gerador.spin_pontos_vida->value());
    proto_retornado->set_max_pontos_vida(gerador.spin_max_pontos_vida->value());
    int aura = gerador.spin_aura->value();
    if (aura > 0) {
      proto_retornado->set_aura(aura);
    } else {
      proto_retornado->clear_aura();
    }
    proto_retornado->set_voadora(gerador.checkbox_voadora->checkState() == Qt::Checked);
    proto_retornado->set_caida(gerador.checkbox_caida->checkState() == Qt::Checked);
    proto_retornado->set_morta(gerador.checkbox_morta->checkState() == Qt::Checked);
    proto_retornado->set_visivel(gerador.checkbox_visibilidade->checkState() == Qt::Checked);
    proto_retornado->set_selecionavel_para_jogador(gerador.checkbox_selecionavel->checkState() == Qt::Checked);
    if (gerador.spin_translacao->value() > 0) {
      proto_retornado->set_translacao_z(gerador.spin_translacao->value());
    } else {
      proto_retornado->clear_translacao_z();
    }
  });
  // TODO: Ao aplicar as mudanças refresca e nao fecha.

  // Cancelar.
  lambda_connect(dialogo, SIGNAL(rejected()), [&notificacao, &proto_retornado] {
      delete proto_retornado;
      proto_retornado = nullptr;
  });
  dialogo->exec();
  delete dialogo;
  return proto_retornado;
}

ent::EntidadeProto* Visualizador3d::AbreDialogoEntidade(
    const ntf::Notificacao& notificacao) {
  if (notificacao.entidade().tipo() == ent::TE_ENTIDADE) {
    return AbreDialogoTipoEntidade(notificacao);
  } else if (notificacao.entidade().tipo() == ent::TE_FORMA) {
    return AbreDialogoTipoForma(notificacao);
  }
  return nullptr;
}

ent::TabuleiroProto* Visualizador3d::AbreDialogoTabuleiro(
    const ntf::Notificacao& notificacao) {
  auto* proto_retornado = new ent::TabuleiroProto;
  ifg::qt::Ui::DialogoIluminacao gerador;
  auto* dialogo = new QDialog(this);
  gerador.setupUi(dialogo);
  const auto& tab_proto = notificacao.tabuleiro();

  // Cor ambiente.
  ent::Cor cor_ambiente_proto(tab_proto.luz_ambiente());
  gerador.botao_cor_ambiente->setStyleSheet(CorParaEstilo(cor_ambiente_proto));
  lambda_connect(gerador.botao_cor_ambiente, SIGNAL(clicked()), [this, dialogo, &gerador, &cor_ambiente_proto] {
    QColor cor =
        QColorDialog::getColor(ProtoParaCor(cor_ambiente_proto), dialogo, QObject::tr("Cor da luz ambiente"));
    if (!cor.isValid()) {
      return;
    }
    gerador.botao_cor_ambiente->setStyleSheet(CorParaEstilo(cor));
    cor_ambiente_proto.CopyFrom(CorParaProto(cor));
  });

  // Cor direcional.
  ent::Cor cor_direcional_proto(tab_proto.luz_direcional().cor());
  gerador.botao_cor_direcional->setStyleSheet(CorParaEstilo(cor_direcional_proto));
  lambda_connect(gerador.botao_cor_direcional, SIGNAL(clicked()), [this, dialogo, &gerador, &cor_direcional_proto] {
    QColor cor =
        QColorDialog::getColor(ProtoParaCor(cor_direcional_proto), dialogo, QObject::tr("Cor da luz ambiente"));
    if (!cor.isValid()) {
      return;
    }
    gerador.botao_cor_direcional->setStyleSheet(CorParaEstilo(cor));
    cor_direcional_proto.CopyFrom(CorParaProto(cor));
  });

  // Posicao na rosa dos ventos. No slider, o zero fica pra baixo enquanto no proto ele fica para direita.
  gerador.dial_posicao->setSliderPosition(tab_proto.luz_direcional().posicao_graus() + 90.0f);
  // Inclinacao: o zero do slider fica para baixo enquanto no proto ele fica para direita.
  gerador.dial_inclinacao->setSliderPosition(tab_proto.luz_direcional().inclinacao_graus() + 90.0f);

  // Textura do tabuleiro.
  gerador.linha_textura->setText(tab_proto.info_textura().id().c_str());
  lambda_connect(gerador.botao_textura, SIGNAL(clicked()),
      [this, dialogo, &gerador ] () {
    QString file_str = QFileDialog::getOpenFileName(this, tr("Abrir textura"), tr(DIR_TEXTURAS, FILTRO_IMAGENS));
    if (file_str.isEmpty()) {
      VLOG(1) << "Operação de leitura de textura cancelada.";
      return;
    }
    gerador.linha_textura->setText(file_str);
  });
  // Ladrilho de textura.
  gerador.checkbox_ladrilho->setCheckState(tab_proto.ladrilho() ? Qt::Checked : Qt::Unchecked);

  // Tamanho.
  gerador.linha_largura->setText(QString::number(tab_proto.largura()));
  gerador.linha_altura->setText(QString::number(tab_proto.altura()));
  lambda_connect(gerador.checkbox_tamanho_automatico, SIGNAL(stateChanged(int)), [this, &gerador] () {
    int novo_estado = gerador.checkbox_tamanho_automatico->checkState();
    // Deve ter textura.
    if (novo_estado == Qt::Checked && gerador.linha_textura->text().isEmpty()) {
      gerador.checkbox_tamanho_automatico->setCheckState(Qt::Unchecked);
      return;
    }
    gerador.linha_largura->setEnabled(novo_estado != Qt::Checked);
    gerador.linha_altura->setEnabled(novo_estado != Qt::Checked);
  });

  // Ao aceitar o diálogo, aplica as mudancas.
  lambda_connect(gerador.botoes, SIGNAL(accepted()),
                 [this, tab_proto, dialogo, &gerador, &cor_ambiente_proto, &cor_direcional_proto, proto_retornado] {
    proto_retornado->mutable_luz_direcional()->set_posicao_graus(gerador.dial_posicao->sliderPosition() - 90.0f);
    proto_retornado->mutable_luz_direcional()->set_inclinacao_graus(gerador.dial_inclinacao->sliderPosition() - 90.0f);
    proto_retornado->mutable_luz_direcional()->mutable_cor()->Swap(&cor_direcional_proto);
    proto_retornado->mutable_luz_ambiente()->Swap(&cor_ambiente_proto);
    if (gerador.linha_textura->text().toStdString() == tab_proto.info_textura().id()) {
      // Textura igual a anterior.
      VLOG(2) << "Textura igual a anterior.";
      proto_retornado->mutable_info_textura()->set_id(tab_proto.info_textura().id());
    } else {
      VLOG(2) << "Textura diferente da anterior.";
      QFileInfo info(gerador.linha_textura->text());
      // TODO fazer uma comparacao melhor. Se o diretorio local terminar com o
      // mesmo nome isso vai falhar.
      if (info.dir().dirName() != DIR_TEXTURAS) {
        VLOG(2) << "Textura local, recarregando.";
        QString id = QString::number(tab_proto.id_cliente()).append(":").append(info.fileName());
        proto_retornado->mutable_info_textura()->set_id(id.toStdString());
        // Usa o id para evitar conflito de textura local com texturas globais.
        // Enviar a textura toda.
        PreencheProtoTextura(info, proto_retornado->mutable_info_textura());
      } else {
        proto_retornado->mutable_info_textura()->set_id(info.fileName().toStdString());
      }
    }
    if (!gerador.linha_textura->text().isEmpty()) {
      proto_retornado->set_ladrilho(gerador.checkbox_ladrilho->checkState() == Qt::Checked);
    } else {
      proto_retornado->clear_ladrilho();
    }
    if (gerador.checkbox_tamanho_automatico->checkState() == Qt::Checked) {
      // Busca tamanho da textura.
      ent::InfoTextura textura = proto_retornado->info_textura();
      if (!textura.has_altura() || !textura.has_largura()) {
        PreencheProtoTextura(IdTexturaParaCaminhoArquivo(textura.id()), &textura);
      }
      proto_retornado->set_largura(textura.largura() / 8);
      proto_retornado->set_altura(textura.altura() / 8);
    } else {
      bool ok = true;
      int largura = gerador.linha_largura->text().toInt(&ok);
      if (!ok) {
        return;
      }
      int altura = gerador.linha_altura->text().toInt(&ok);
      if (!ok) {
        return;
      }
      proto_retornado->set_largura(largura);
      proto_retornado->set_altura(altura);
    }
    VLOG(1) << "Retornando tabuleiro: " << proto_retornado->ShortDebugString();
    dialogo->accept();
  });
  // Cancelar.
  lambda_connect(dialogo, SIGNAL(rejected()), [&notificacao, &proto_retornado] {
    delete proto_retornado;
    proto_retornado = nullptr;
  });
  dialogo->exec();
  delete dialogo;
  return proto_retornado;
}

ent::OpcoesProto* Visualizador3d::AbreDialogoOpcoes(
    const ntf::Notificacao& notificacao) {
  auto* proto_retornado = new ent::OpcoesProto;
  ifg::qt::Ui::DialogoOpcoes gerador;
  auto* dialogo = new QDialog(this);
  gerador.setupUi(dialogo);
  const auto& opcoes_proto = notificacao.opcoes();

  // fps.
  gerador.checkbox_mostrar_fps->setCheckState(opcoes_proto.mostra_fps() ? Qt::Checked : Qt::Unchecked);
  // Texturas de frente.
  gerador.checkbox_texturas_sempre_de_frente->setCheckState(
      opcoes_proto.texturas_sempre_de_frente() ? Qt::Checked : Qt::Unchecked);
  // Iluminacao mestre.
  gerador.checkbox_iluminacao_mestre->setCheckState(
      opcoes_proto.iluminacao_mestre_igual_jogadores() ? Qt::Checked : Qt::Unchecked);
  // Rosa dos ventos.
  gerador.checkbox_rosa_dos_ventos->setCheckState(
      opcoes_proto.desenha_rosa_dos_ventos() ? Qt::Checked : Qt::Unchecked);

  // Ao aceitar o diálogo, aplica as mudancas.
  lambda_connect(dialogo, SIGNAL(accepted()), [dialogo, &gerador, proto_retornado] {
    proto_retornado->set_mostra_fps(gerador.checkbox_mostrar_fps->checkState() == Qt::Checked ? true : false);
    proto_retornado->set_texturas_sempre_de_frente(
        gerador.checkbox_texturas_sempre_de_frente->checkState() == Qt::Checked ? true : false);
    proto_retornado->set_iluminacao_mestre_igual_jogadores(
        gerador.checkbox_iluminacao_mestre->checkState() == Qt::Checked ? true : false);
    proto_retornado->set_desenha_rosa_dos_ventos(
        gerador.checkbox_rosa_dos_ventos->checkState() == Qt::Checked ? true : false);
  });
  // Cancelar.
  lambda_connect(dialogo, SIGNAL(rejected()), [&notificacao, &proto_retornado] {
    delete proto_retornado;
    proto_retornado = nullptr;
  });
  dialogo->exec();
  delete dialogo;
  return proto_retornado;
}

void Visualizador3d::TrataAcaoTemporizadaTeclado() {
  // Busca primeira tecla.
  if (teclas_.empty()) {
    LOG(ERROR) << "Temporizador sem teclas";
    return;
  }
  int primeira_tecla = *teclas_.begin();
  switch (primeira_tecla) {
    case Qt::Key_A: {
      if (teclas_.size() == 1) {
        return;
      }
      if (teclas_[1] == Qt::Key_Delete) {
        tabuleiro_->LimpaListaPontosVida();
      } else if (teclas_[1] == Qt::Key_Backspace) {
        tabuleiro_->LimpaUltimoListaPontosVida();
      } else {
        auto lista_dano = CalculaDano(teclas_.begin() + 1, teclas_.end());
        if (teclas_[1] == Qt::Key_D) {
          // Inverte o dano.
          for (int& pv : lista_dano) {
            pv = -pv;
          }
        }
        tabuleiro_->AcumulaPontosVida(lista_dano);
      }
      break;
    }
    case Qt::Key_C:
    case Qt::Key_D: {
      auto lista_pv = CalculaDano(teclas_.begin(), teclas_.end());
      if (lista_pv.size() != 1) {
        break;
      }
      if (primeira_tecla == Qt::Key_D) {
        lista_pv[0] = -lista_pv[0];
      }
      tabuleiro_->TrataAcaoAtualizarPontosVidaEntidades(lista_pv[0]);
      break;
    }
    default:
      VLOG(1) << "Tecla de temporizador nao reconhecida: " << primeira_tecla;
  }
}

void Visualizador3d::TrataAcaoTemporizadaMouse() {
  VLOG(1) << "Tratando acao temporizada de mouse em: " << ultimo_x_ << ", " << ultimo_y_;
  tabuleiro_->TrataMouseParadoEm(ultimo_x_, ultimo_y_);
}

void Visualizador3d::MudaEstado(estado_e novo_estado) {
  if (novo_estado == ESTADO_TEMPORIZANDO_MOUSE) {
    temporizador_mouse_ = MAX_TEMPORIZADOR_MOUSE;
  } else if (novo_estado == ESTADO_TEMPORIZANDO_TECLADO) {
    teclas_.clear();
    temporizador_teclado_ = MAX_TEMPORIZADOR_TECLADO;
  }
  VLOG(2) << "Mudando para estado: " << novo_estado;
  estado_ = novo_estado;
}
}  // namespace qt
}  // namespace ifg
