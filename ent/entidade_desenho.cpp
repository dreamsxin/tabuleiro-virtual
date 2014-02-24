/** Implementacao dos varios tipos de desenho da classe Entidade. */

#include <cmath>
#if __APPLE__
#include <OpenGL/gl.h>
#include <GLUT/glut.h>
#else
#include <GL/gl.h>
#include <GL/glut.h>
#endif

#include "ent/constantes.h"
#include "ent/entidade.h"
#include "ent/util.h"
#include "log/log.h"

namespace ent {

void Entidade::DesenhaObjeto(ParametrosDesenho* pd, const float* matriz_shear) {
  switch (proto_.tipo()) {
    case TE_ENTIDADE:
      DesenhaObjetoEntidade(pd, matriz_shear);
      return;
    case TE_FORMA:
      DesenhaObjetoForma(pd, matriz_shear);
      return;
  }
}

void Entidade::DesenhaObjetoEntidade(ParametrosDesenho* pd, const float* matriz_shear) {
  if (!proto_.has_info_textura()) {
    glPushMatrix();
    MontaMatriz(true, matriz_shear);
    glutSolidCone(TAMANHO_LADO_QUADRADO_2 - 0.2, ALTURA, NUM_FACES, NUM_LINHAS);
    glTranslated(0, 0, ALTURA);
    glutSolidSphere(TAMANHO_LADO_QUADRADO_2 - 0.4, NUM_FACES, NUM_FACES);
    glPopMatrix();
    return;
  }

  // tijolo da base (altura TAMANHO_LADO_QUADRADO_10).
  {
    glPushMatrix();
    MontaMatriz(false, matriz_shear);
    glTranslated(0.0, 0.0, TAMANHO_LADO_QUADRADO_10 / 2);
    glScalef(0.8f, 0.8f, TAMANHO_LADO_QUADRADO_10 / 2);
    if (pd->entidade_selecionada()) {
      glRotatef(rotacao_disco_selecao_graus_, 0, 0, 1.0f);
    }
    glutSolidCube(TAMANHO_LADO_QUADRADO);
    glPopMatrix();
  }

  glPushMatrix();
  MontaMatriz(true, matriz_shear);
  // Tijolo da moldura.
  if (proto_.achatado()) {
    glTranslated(0.0, 0.0, TAMANHO_LADO_QUADRADO_10);
    if (pd->entidade_selecionada()) {
      glRotatef(rotacao_disco_selecao_graus_, 0, 0, 1.0f);
    }
    glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
    glScalef(0.8f, 1.0f, 0.8f);
  } else {
    // Moldura da textura: acima do tijolo de base e achatado em Y (longe da camera).
    glTranslated(0, 0, TAMANHO_LADO_QUADRADO_2 + TAMANHO_LADO_QUADRADO_10);
    double angulo = 0;
    // So desenha a textura de frente pra entidades nao caidas.
    if (pd->texturas_sempre_de_frente() && !proto_.caida()) {
      double dx = X() - pd->pos_olho().x();
      double dy = Y() - pd->pos_olho().y();
      double r = sqrt(pow(dx, 2) + pow(dy, 2));
      angulo = (acos(dx / r) * RAD_PARA_GRAUS);
      if (dy < 0) {
        // A funcao asin tem dois resultados mas sempre retorna o positivo [0, PI].
        // Se o vetor estiver nos quadrantes de baixo, inverte o angulo.
        angulo = -angulo;
      }
      glRotated(angulo - 90.0f, 0, 0, 1.0);
    }
    glPushMatrix();
    glScalef(1.0f, 0.1f, 1.0f);
    glutSolidCube(TAMANHO_LADO_QUADRADO);
    glPopMatrix();
  }

  // Tela onde a textura será desenhada face para o sul (nao desenha para sombra).
  GLuint id_textura = pd->desenha_texturas() && proto_.has_info_textura() ?
    texturas_->Textura(proto_.info_textura().id()) : GL_INVALID_VALUE;
  if (matriz_shear == nullptr && id_textura != GL_INVALID_VALUE) {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, id_textura);
    glNormal3f(0.0f, -1.0f, 0.0f);
    Cor c;
    c.set_r(1.0f);
    c.set_g(1.0f);
    c.set_b(1.0f);
    c.set_a(pd->has_alpha_translucidos() ? pd->alpha_translucidos() : 1.0f);
    MudaCor(proto_.morta() ? EscureceCor(c) : c);
    glBegin(GL_QUADS);
    // O openGL assume que o (0.0, 0.0) da textura eh embaixo,esquerda. O QT retorna os dados da
    // imagem com origem em cima esquerda. Entao a gente mapeia a textura com o eixo vertical invertido.
    // O quadrado eh desenhado EB, DB, DC, EC. A textura eh aplicada: EC, DC, DB, EB.
    glTexCoord2f(0.0f, 1.0f);
    glVertex3f(
        -TAMANHO_LADO_QUADRADO_2, -TAMANHO_LADO_QUADRADO_2 / 10.0f - 0.01f, -TAMANHO_LADO_QUADRADO_2);
    glTexCoord2f(1.0f, 1.0f);
    glVertex3f(
        TAMANHO_LADO_QUADRADO_2, -TAMANHO_LADO_QUADRADO_2 / 10.0f - 0.01f, -TAMANHO_LADO_QUADRADO_2);
    glTexCoord2f(1.0f, 0.0f);
    glVertex3f(
        TAMANHO_LADO_QUADRADO_2, -TAMANHO_LADO_QUADRADO_2 / 10.0f - 0.01f, TAMANHO_LADO_QUADRADO_2);
    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(
        -TAMANHO_LADO_QUADRADO_2, -TAMANHO_LADO_QUADRADO_2 / 10.0f - 0.01f, TAMANHO_LADO_QUADRADO_2);
    glEnd();
    glDisable(GL_TEXTURE_2D);
  }
  glPopMatrix();
}

void Entidade::DesenhaObjetoForma(ParametrosDesenho* pd, const float* matriz_shear) {
  glPushAttrib(GL_ENABLE_BIT);
  if (proto_.cor().a() < 1.0f) {
    glEnable(GL_BLEND);
  }
  glEnable(GL_NORMALIZE);
  glDisable(GL_CULL_FACE);
  MudaCor(proto_.cor());
  switch (proto_.sub_tipo()) {
    case TF_RETANGULO: {
      glNormal3i(0, 0, 1);
      glRectf(proto_.inicio().x(), proto_.inicio().y(), proto_.fim().x(), proto_.fim().y());
    }
    break;
    case TF_ESFERA: {
      // Usar x como base para achatamento.
      float centro_x = (proto_.inicio().x() + proto_.fim().x()) / 2.0f;
      float centro_y = (proto_.inicio().y() + proto_.fim().y()) / 2.0f;
      float escala_x = fabs(proto_.inicio().x() - proto_.fim().x());
      float escala_y = fabs(proto_.inicio().y() - proto_.fim().y());
      glPushMatrix();
      glTranslatef(centro_x, centro_y, 0.0f);
      glScalef(escala_x, escala_y, std::min(escala_x, escala_y));
      glutSolidSphere(0.5f  /*raio*/, 20  /*ao redor*/, 20 /*vertical*/);
      glPopMatrix();
    }
    break;
    case TF_CIRCULO: {
      glNormal3i(0, 0, 1);
      // Usar x como base para achatamento.
      float centro_x = (proto_.inicio().x() + proto_.fim().x()) / 2.0f;
      float centro_y = (proto_.inicio().y() + proto_.fim().y()) / 2.0f;
      float escala_x = fabs(proto_.inicio().x() - proto_.fim().x());
      float escala_y = fabs(proto_.inicio().y() - proto_.fim().y());
      glPushMatrix();
      glTranslatef(centro_x, centro_y, 0.0f);
      glScalef(escala_x, escala_y, 1.0f);
      DesenhaDisco(0.5f, 12);
      glPopMatrix();
    }
    break;
    case TF_CUBO: {
      // Usar x como base para achatamento.
      float centro_x = (proto_.inicio().x() + proto_.fim().x()) / 2.0f;
      float centro_y = (proto_.inicio().y() + proto_.fim().y()) / 2.0f;
      float escala_x = fabs(proto_.inicio().x() - proto_.fim().x());
      float escala_y = fabs(proto_.inicio().y() - proto_.fim().y());
      glPushMatrix();
      glTranslatef(centro_x, centro_y, TAMANHO_LADO_QUADRADO_2);
      // Altura do cubo do lado do quadrado.
      glScalef(escala_x, escala_y, TAMANHO_LADO_QUADRADO);
      glutSolidCube(1.0f);
      glPopMatrix();
    }
    break;
    case TF_LIVRE: {
      glNormal3i(0, 0, 1);
      bool usa_stencil = pd->transparencias() && proto_.cor().a() < 1.0f;
      if (usa_stencil) {
        LigaStencil();
      }
      DesenhaLinha3d(proto_.ponto(), TAMANHO_LADO_QUADRADO / 2.0f);
      if (usa_stencil) {
        DesenhaStencil(proto_.cor());
      }
    }
    break;
    default:
      LOG(ERROR) << "Forma de desenho invalida";
  }
  glPopAttrib();
}

}  // namespace ent
