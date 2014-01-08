#include <stdexcept>
#include <cmath>

#include <QBoxLayout>
#include <QColorDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QMouseEvent>
#include <QString>
#include <GL/gl.h>

#include "ent/tabuleiro.h"
#include "ifg/qt/util.h"
#include "ifg/qt/visualizador3d.h"
#include "ifg/qt/ui/entidade.h"
#include "log/log.h"
#include "ntf/notificacao.pb.h"

using namespace ifg::qt;
using namespace std;

namespace {

ent::botao_e MapeiaBotao(Qt::MouseButton botao) {
  switch (botao) {
    case Qt::LeftButton: return ent::BOTAO_ESQUERDO;
    case Qt::RightButton: return ent::BOTAO_DIREITO;
    case Qt::MidButton: return ent::BOTAO_MEIO;
    default: return ent::BOTAO_NENHUM; 
  }
}

}  // namespace

Visualizador3d::Visualizador3d(
    ntf::CentralNotificacoes* central, ent::Tabuleiro* tabuleiro, QWidget* pai) 
    :  QGLWidget(QGLFormat(QGL::DepthBuffer | QGL::Rgba | QGL::DoubleBuffer), pai),
       central_(central), tabuleiro_(tabuleiro) {
  central_->RegistraReceptor(this);
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

namespace {

int ConverteCor(float cor_float) {
  int cor = static_cast<int>(255.0f * cor_float);
  if (cor < 0) {
    LOG(WARNING) << "Cor menor que zero!";
    cor = 0;
  } else if (cor > 255) {
    LOG(WARNING) << "Cor maior que 255.";
    cor = 255;
  }
  return cor;
}

float ConverteCor(int cor_int) {
  return cor_int / 255.0;
}

// Converte do formato ent::Proto para cor do QT.
const QColor ProtoParaCor(const ent::Cor& cor) {
  return QColor(ConverteCor(cor.r()), 
                ConverteCor(cor.g()),
                ConverteCor(cor.b()),
                ConverteCor(cor.a()));
}

// Converte cor do QT para ent::Cor.
const ent::Cor CorParaProto(const QColor& qcor) {
  ent::Cor cor;
  cor.set_r(ConverteCor(qcor.red()));
  cor.set_g(ConverteCor(qcor.green()));
  cor.set_b(ConverteCor(qcor.blue()));
  cor.set_a(ConverteCor(qcor.alpha()));
  return cor;
}

// Retorna uma string de estilo para background-color baseada na cor passada.
const QString CorParaEstilo(const QColor& cor) {
  QString estilo_fmt("background-color: rgb(%1, %2, %3);");
  QString estilo = estilo_fmt.arg(cor.red()).arg(cor.green()).arg(cor.blue());
  VLOG(1) << "Retornando estilo: " << estilo.toStdString();
  return estilo;
}

const QString CorParaEstilo(const ent::Cor& cor) {
  return CorParaEstilo(ProtoParaCor(cor));
}

/** Abre um diálogo editável com as características da entidade. */ 
ent::EntidadeProto* AbreDialogoEntidade(const ntf::Notificacao& notificacao, QWidget* pai) {
  auto* proto = new ent::EntidadeProto(notificacao.entidade());
  ifg::qt::Ui::DialogoEntidade gerador;
  auto* dialogo = new QDialog(pai);
  gerador.setupUi(dialogo);
  QString id_str;
  gerador.campo_id->setText(id_str.setNum(proto->id()));
  // Cor da entidade.
  ent::EntidadeProto ent_cor;
  ent_cor.mutable_cor()->CopyFrom(proto->cor());
  gerador.botao_cor->setStyleSheet(CorParaEstilo(proto->cor()));
  if (proto->has_cor()) {
    gerador.checkbox_cor->setCheckState(Qt::Checked);
  } else {
    gerador.checkbox_cor->setCheckState(Qt::Unchecked);
  }
  lambda_connect(gerador.botao_cor, SIGNAL(clicked()), [dialogo, &gerador, &ent_cor] {
    QColor cor =
        QColorDialog::getColor(ProtoParaCor(ent_cor.cor()), dialogo, QObject::tr("Cor do objeto"));
    if (!cor.isValid()) {
      return;
    }
    gerador.checkbox_cor->setCheckState(Qt::Checked);
    gerador.botao_cor->setStyleSheet(CorParaEstilo(cor));
    ent_cor.mutable_cor()->CopyFrom(CorParaProto(cor));
  });
  // Cor da luz.
  ent::EntidadeProto luz_cor;
  luz_cor.mutable_cor()->CopyFrom(proto->luz().cor());
  gerador.botao_luz->setStyleSheet(CorParaEstilo(proto->luz().cor()));
  if (proto->has_luz()) {
    gerador.checkbox_luz->setCheckState(Qt::Checked);
  } else {
    gerador.checkbox_luz->setCheckState(Qt::Unchecked);
  }
  lambda_connect(gerador.botao_luz, SIGNAL(clicked()), [dialogo, &gerador, &luz_cor] {
    QColor cor =
        QColorDialog::getColor(ProtoParaCor(luz_cor.cor()), dialogo, QObject::tr("Cor da luz"));
    if (!cor.isValid()) {
      return;
    }
    luz_cor.mutable_cor()->CopyFrom(CorParaProto(cor));
    gerador.botao_luz->setStyleSheet(CorParaEstilo(cor));
    gerador.checkbox_luz->setCheckState(Qt::Checked);
  });
  // Ao aceitar o diálogo, aplica as mudancas.
  lambda_connect(gerador.botoes, SIGNAL(accepted()), [dialogo, &gerador, &proto, &ent_cor, &luz_cor] {
    if (gerador.checkbox_cor->checkState() == Qt::Checked) {
      proto->mutable_cor()->Swap(ent_cor.mutable_cor());
    } else {
      proto->clear_cor();
    }
    if (gerador.checkbox_luz->checkState() == Qt::Checked) {
      proto->mutable_luz()->mutable_cor()->Swap(luz_cor.mutable_cor());
    } else {
      proto->clear_luz();
    }
    dialogo->accept();
  });
  // Cancelar.
  lambda_connect(gerador.botoes, SIGNAL(rejected()), [&notificacao, &proto] {
      delete proto;
      proto = nullptr;
  });
  dialogo->exec();
  delete dialogo;
  return proto;
}

}  // namespace

// notificacao
bool Visualizador3d::TrataNotificacao(const ntf::Notificacao& notificacao) {
  switch (notificacao.tipo()) {
    case ntf::TN_INICIADO:
      // chama o resize pra iniciar a geometria e desenha a janela
      resizeGL(width(), height());
      glDraw();
      break;
    case ntf::TN_ABRIR_DIALOGO_ENTIDADE: {
      auto* entidade = AbreDialogoEntidade(notificacao, this);
      if (entidade == nullptr) {
        VLOG(1) << "Alterações descartadas";
        break;
      }
      auto* n = new ntf::Notificacao;
      n->set_tipo(ntf::TN_ATUALIZAR_ENTIDADE);
      n->mutable_entidade()->Swap(entidade);
      central_->AdicionaNotificacao(n);
      break;
    }
    default: ;
  }
  // Sempre redesenha para evitar qualquer problema de atualizacao.
  glDraw();
  return true;
}

// mouse

void Visualizador3d::mousePressEvent(QMouseEvent* event) {
  int altura = height();
  double aspecto = static_cast<double>(width()) / altura;
  tabuleiro_->TrataBotaoPressionado(
    MapeiaBotao(event->button()), 
    event->x(), altura - event->y(), aspecto);
  event->accept();
  glDraw();
}

void Visualizador3d::mouseReleaseEvent(QMouseEvent* event) {
  tabuleiro_->TrataBotaoLiberado();
  event->accept();
  glDraw();
}

void Visualizador3d::mouseDoubleClickEvent(QMouseEvent* event) {
  int altura = height();
  double aspecto = static_cast<double>(width()) / altura;
  tabuleiro_->TrataDuploClick(
    MapeiaBotao(event->button()), 
    event->x(), altura - event->y(), aspecto);
  event->accept();
}

void Visualizador3d::mouseMoveEvent(QMouseEvent* event) {
  tabuleiro_->TrataMovimento(event->x(), (height() - event->y()));
  event->accept();
  glDraw();
}

void Visualizador3d::wheelEvent(QWheelEvent* event) {
  tabuleiro_->TrataRodela(event->delta());
  event->accept();
  glDraw();
}















