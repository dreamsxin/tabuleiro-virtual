/** Baseado no SanAngeles demo application. So uma casca Java para chamar o codigo nativo. */
package com.matferib.Tabuleiro;

import java.util.Vector;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.opengl.GLSurfaceView;
import android.os.Bundle;
import android.util.Log;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.ScaleGestureDetector;

// Atividade do tabuleiro que possui o view do OpenGL.
public class TabuleiroActivity extends Activity {
  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    Intent intencao = getIntent();
    mGLView = new TabuleiroSurfaceView(this, getIntent().getStringExtra(SelecaoActivity.MENSAGEM_EXTRA));
    setContentView(mGLView);
  }

  @Override
  protected void onPause() {
    super.onPause();
    mGLView.onPause();
  }

  @Override
  protected void onResume() {
    super.onResume();
    mGLView.onResume();
  }

  private GLSurfaceView mGLView;

  static {
    System.loadLibrary("tabuleiro");
  }
}

// View do OpenGL.
class TabuleiroSurfaceView extends GLSurfaceView {
  public TabuleiroSurfaceView(Context context, String endereco) {
    super(context);
    renderer_ = new TabuleiroRenderer(this, endereco, getResources().getAssets());
    detectorEventos_ = new GestureDetector(context, renderer_);
    detectorEventos_.setOnDoubleTapListener(renderer_);
    detectorEscala_ = new ScaleGestureDetector(context, renderer_);
    detectorEscala_.setQuickScaleEnabled(true);
    detectorRotacao_ = new RotationGestureDetector(renderer_);
    detectorTranslacao_ = new TranslationGestureDetector(renderer_);
    setRenderer(renderer_);
    setRenderMode(GLSurfaceView.RENDERMODE_WHEN_DIRTY);
  }

  public boolean onTouchEvent(final MotionEvent event) {
    if ((event.getActionMasked() & MotionEvent.ACTION_UP) > 0) {
      renderer_.onUp(event);
    }
    if (event.getPointerCount() <= 1) {
      detectorEventos_.onTouchEvent(event);
    } else if (event.getPointerCount() == 2) {
      detectorRotacao_.onTouch(event);
      detectorEscala_.onTouchEvent(event);
      detectorTranslacao_.onTouch(event);
    } else if (event.getPointerCount() == 3) {
      detectorTranslacao_.onTouch(event);
    }
    return true;
  }

  @Override
  public void onPause() {
    super.onPause();
    nativePause();
  }

  @Override
  public void onResume() {
    super.onResume();
    nativeResume();
  }

  private TabuleiroRenderer renderer_;
  private GestureDetector detectorEventos_;
  private ScaleGestureDetector detectorEscala_;
  private RotationGestureDetector detectorRotacao_;
  private TranslationGestureDetector detectorTranslacao_;
  private static native void nativePause();
  private static native void nativeResume();
}

// Renderizador do tabuleiro. Responsavel pelo timer que atualiza o tabuleiro.
class TabuleiroRenderer extends java.util.TimerTask
    implements GLSurfaceView.Renderer,
               Runnable,
               GestureDetector.OnGestureListener,
               GestureDetector.OnDoubleTapListener,
               ScaleGestureDetector.OnScaleGestureListener,
               RotationGestureDetector.RotationListener,
               TranslationGestureDetector.TranslationListener {

  public static final String TAG = "TabuleiroRenderer";

  public TabuleiroRenderer(GLSurfaceView parent, String endereco, android.content.res.AssetManager assets) {
    endereco_ = endereco;
    assets_ = assets;
    parent_ = parent;
    //last_x_ = last_y_ = 0;
  }

  public void onSurfaceCreated(GL10 gl, EGLConfig config) {
    nativeInit(endereco_, assets_);
    // Liga o timer a cada 33 ms. TODO: funcao nativa para retornar o periodo do timer.
    java.util.Timer timer = new java.util.Timer();
    timer.scheduleAtFixedRate(this, 0, 33);
  }

  public void onSurfaceChanged(GL10 gl, int w, int h) {
    nativeResize(w, h);
  }

  /** Remove os eventos duplicados de um tipo. */
  private void removeEventosDuplicados(int tipo, Vector<Evento> eventos) {
    // Remove os eventos consecutivos de movimento.
    Vector<Evento> eventosSemDuplicados = new Vector<Evento>();
    Evento ultimo = null;
    for (Evento evento :  eventos) {
      if (evento.tipo() == tipo) {
        ultimo = evento;
      } else {
        if (ultimo != null) {
          // Adiciona o ultimo movimento.
          eventosSemDuplicados.add(ultimo);
          ultimo = null;
        }
        eventosSemDuplicados.add(evento);
      }
    }
    // Poe o ultimo movimento se ele foi o ultimo evento.
    if (ultimo != null) {
      // Adiciona o ultimo movimento.
      eventosSemDuplicados.add(ultimo);
      ultimo = null;
    }
    eventos.clear();
    eventos.addAll(eventosSemDuplicados);
  }

  /** Junta as translacoes em uma so. */
  private void juntaTranslacoes(Vector<Evento> eventos) {
    // Remove os eventos consecutivos de movimento.
    boolean primeiraTranslacao = true;
    Vector<Evento> eventosSemDuplicados = new Vector<Evento>();
    int x = 0, y = 0, nx = 0, ny = 0;
    for (Evento evento :  eventos) {
      if (evento.tipo() == Evento.TRANSLACAO) {
        if (primeiraTranslacao) {
          primeiraTranslacao = false;
          x = evento.x();
          y = evento.y();
        }
        nx = evento.nx();
        ny = evento.ny();
      } else {
        eventosSemDuplicados.add(evento);
      }
    }
    if (!primeiraTranslacao) {
      eventosSemDuplicados.add(Evento.Translacao(x, y, nx, ny));
    }
    eventos.clear();
    eventos.addAll(eventosSemDuplicados);
  }

  /** Toda atualizacao eh feita daqui para acontecer na mesma thread que o grafico. */
  public void onDrawFrame(GL10 gl) {
    //Log.d(TAG, "DrawFrame");
    nativeTimer();
    //Log.d(TAG, "Tam Evento Antes: " + eventos_.size());
    Vector<Evento> eventos;
    synchronized (eventos_) {
      eventos = new Vector<Evento>(eventos_);
      eventos_.clear();
    }
    removeEventosDuplicados(Evento.MOVIMENTO, eventos);
    juntaTranslacoes(eventos);

    //Log.d(TAG, "Tam Evento Depois: " + eventosSemMovimentosDuplicados.size());
    for (Evento evento :  eventos) {
      Log.d(TAG, "Evento: " + evento.toString());
      switch (evento.tipo()) {
        case Evento.TRANSLACAO:
          nativeTranslation(evento.x(), evento.y(), evento.nx(), evento.ny());
          break;
        case Evento.ESCALA:
          nativeScale(evento.escala());
          break;
        case Evento.ROTACAO:
          nativeRotation(evento.rotacao());
          break;
        case Evento.CLIQUE:
          nativeTouchPressed(evento.x(), evento.y());
          nativeTouchReleased();
          break;
        case Evento.CLIQUE_DUPLO:
          nativeDoubleClick(evento.x(), evento.y());
          break;
        case Evento.DETALHAMENTO:
          nativeHover(evento.x(), evento.y());
          break;
        case Evento.PRESSIONADO:
          nativeTouchPressed(evento.x(), evento.y());
          break;
        case Evento.LIBERADO:
          nativeTouchReleased();
          break;
        case Evento.MOVIMENTO:
          nativeTouchMoved(evento.x(), evento.y());
          break;
        default:
      }
    }
    nativeRender();
  }

  /** Sera chamada pelo timer a cada intervalo de atualizacao. Pode vir de qualquer thread. */
  public void run() {
    parent_.requestRender();
  }

  // Detector de eventos.
  @Override
  public boolean onDown(MotionEvent event) {
    Log.d(TAG, "Down");
    return true;
  }

  // Algum ponteiro terminou.
  public boolean onUp(MotionEvent event) {
    Log.d(TAG, "Up");
    eventos_.add(Evento.Liberado());
    carregando_ = false;
    return true;
  }

  @Override
  public boolean onFling(MotionEvent event1, MotionEvent event2,
      float velocityX, float velocityY) {
    Log.d(TAG, "Fling");
    return true;
  }

  @Override
  public void onLongPress(MotionEvent event) {
    Log.d(TAG, "LongPress");
    eventos_.add(Evento.Detalhamento((int)event.getX(), (int)(parent_.getHeight() - event.getY())));
  }

  @Override
  public boolean onScroll(MotionEvent e1, MotionEvent e2, float distanceX, float distanceY) {
    Log.d(TAG, "Scroll");
    if (!carregando_) {
      eventos_.add(Evento.Pressionado((int)e1.getX(), (int)(parent_.getHeight() - e1.getY())));
      carregando_ = true;
    }
    eventos_.add(Evento.Movimento((int)e2.getX(), (int)(parent_.getHeight() - e2.getY())));
    return true;
  }

  @Override
  public void onShowPress(MotionEvent event) {
    Log.d(TAG, "ShowPress");
    eventos_.add(Evento.Toque((int)event.getX(), (int)(parent_.getHeight() - event.getY())));
  }

  @Override
  public boolean onSingleTapUp(MotionEvent event) {
    Log.d(TAG, "TapUp");
    return true;
  }

  @Override
  public boolean onDoubleTap(MotionEvent event) {
    Log.d(TAG, "DoubleTap");
    eventos_.add(Evento.DuploClique((int)event.getX(), (int)(parent_.getHeight() - event.getY())));
    return true;
  }

  @Override
  public boolean onDoubleTapEvent(MotionEvent event) {
    Log.d(TAG, "DoubleTapEvent");
    return true;
  }

  @Override
  public boolean onSingleTapConfirmed(MotionEvent event) {
    Log.d(TAG, "SingleTapConfirmed");
    eventos_.add(Evento.Clique((int)event.getX(), (int)(parent_.getHeight() - event.getY())));
    return true;
  }

  // Detector de escala.
  @Override
  public boolean onScale(ScaleGestureDetector detector) {
    Log.d(TAG, "Scale");
    eventos_.add(Evento.Escala(detector.getScaleFactor()));
    return true;
  }

  @Override
  public boolean onScaleBegin(ScaleGestureDetector detector) {
    Log.d(TAG, "ScaleBegin");
    return true;
  }

  @Override
  public void onScaleEnd(ScaleGestureDetector detector) {
    Log.d(TAG, "ScaleEnd");
  }

  // Detector rotacao.
  @Override
  public void onRotate(float delta) {
    if (delta == 0.0f) {
      return;
    }
    Log.d(TAG, "Rotate");
    eventos_.add(Evento.Rotacao(delta));
  }

  // Detector de translacao.
  @Override
  public void onTranslate(int x, int y, int nx, int ny) {
    Log.d(TAG, "Translation");
    eventos_.add(Evento.Translacao(x, (int)(parent_.getHeight() - y), nx, (int)(parent_.getHeight() - ny)));
  }

  private static native void nativeInit(String endereco, Object assets);
  private static native void nativeResize(int w, int h);
  private static native void nativeRender();
  private static native void nativeDone();
  private static native void nativeTimer();
  private static native void nativeDoubleClick(int x, int y);
  private static native void nativeTouchPressed(int x, int y);
  private static native void nativeTouchMoved(int x, int y);
  private static native void nativeTouchReleased();
  private static native void nativeHover(int x, int y);
  private static native void nativeScale(float s);
  private static native void nativeRotation(float r);
  private static native void nativeTranslation(int x, int y, int nx, int ny);

  private GLSurfaceView parent_;

  private Vector<Evento> eventos_ = new Vector<Evento>();
  private boolean carregando_ = false;
  private String endereco_;
  private android.content.res.AssetManager assets_;
}

// Copiado de:
// https://code.google.com/p/osmdroid/source/browse/trunk/
// OpenStreetMapViewer/src/org/osmdroid/RotationGestureDetector.java?r=1186
class RotationGestureDetector {
  public interface RotationListener {
    // Angulo em radianos.
    public void onRotate(float deltaAngle);
  }

  protected float mRotation;
  private RotationListener mListener;

  public RotationGestureDetector(RotationListener listener) {
    mListener = listener;
  }

  private float rotation(MotionEvent event) {
    double delta_x = (event.getX(0) - event.getX(1));
    double delta_y = (event.getY(0) - event.getY(1));
    double radians = Math.atan2(delta_y, delta_x);
    return (float)radians;
  }

  public void onTouch(MotionEvent e) {
    if (e.getPointerCount() != 2)
      return;

    if (e.getActionMasked() == MotionEvent.ACTION_POINTER_DOWN) {
      mRotation = rotation(e);
    }

    float rotation = rotation(e);
    float delta = rotation - mRotation;
    mRotation += delta;
    mListener.onRotate(delta);
  }
}

// Translacao com 3 dedos.
class TranslationGestureDetector {
  public interface TranslationListener {
    public void onTranslate(int x, int y, int nx, int ny);
  }

  public TranslationGestureDetector(TranslationListener ouvinte) {
    ouvinte_ = ouvinte;
  }

  public void onTouch(MotionEvent e) {
    if (e.getActionMasked() == MotionEvent.ACTION_POINTER_DOWN) {
      ultimo_x_ = X(e);
      ultimo_y_ = Y(e);
      return;
    } else if (e.getActionMasked() == MotionEvent.ACTION_POINTER_UP) {
      return;
    }
    int nx = X(e);
    int ny = Y(e);
    if (nx != ultimo_x_ || ny != ultimo_y_) {
      ouvinte_.onTranslate(ultimo_x_, ultimo_y_, nx, ny);
    }
    ultimo_x_ = X(e);
    ultimo_y_ = Y(e);
  }

  private int X(MotionEvent e) {
    int soma = 0;
    for (int i = 0; i < e.getPointerCount(); ++i) {
      soma += e.getX(i);
    }
    return soma / e.getPointerCount();
  }

  private int Y(MotionEvent e) {
    int soma = 0;
    for (int i = 0; i < e.getPointerCount(); ++i) {
      soma += e.getY(i);
    }
    return soma / e.getPointerCount();
  }

  private TranslationListener ouvinte_;
  private int ultimo_x_, ultimo_y_;
}

/** Os tipos de eventos tratados pelo tabuleiro. */
class Evento {
  public static final int CLIQUE = 1;
  public static final int CLIQUE_DUPLO = 2;
  public static final int ESCALA = 3;
  public static final int TOQUE = 4;
  public static final int CARREGAR = 5;
  public static final int DETALHAMENTO = 6;
  public static final int PRESSIONADO = 7;
  public static final int LIBERADO = 8;
  public static final int MOVIMENTO = 9;
  public static final int ROTACAO = 10;
  public static final int TRANSLACAO = 11;

  public static Evento Liberado() {
    return new Evento(LIBERADO);
  }

  public static Evento Translacao(int x,  int y, int nx, int ny) {
    Evento evento = new Evento(TRANSLACAO);
    evento.x_ = x;
    evento.y_ = y;
    evento.nx_ = nx;
    evento.ny_ = ny;
    return evento;
  }

  public static Evento Escala(float escala) {
    Evento evento = new Evento(ESCALA);
    evento.escala_ = escala;
    return evento;
  }

  public static Evento Rotacao(float rotacao) {
    Evento evento = new Evento(ROTACAO);
    evento.rotacao_ = rotacao;
    return evento;
  }

  public static Evento Pressionado(int x,  int y) {
    Evento evento = new Evento(PRESSIONADO);
    evento.x_ = x;
    evento.y_ = y;
    return evento;
  }

  public static Evento Movimento(int x,  int y) {
    Evento evento = new Evento(MOVIMENTO);
    evento.x_ = x;
    evento.y_ = y;
    return evento;
  }

  public static Evento Clique(int x,  int y) {
    Evento evento = new Evento(CLIQUE);
    evento.x_ = x;
    evento.y_ = y;
    return evento;
  }

  public static Evento DuploClique(int x,  int y) {
    Evento evento = new Evento(CLIQUE_DUPLO);
    evento.x_ = x;
    evento.y_ = y;
    return evento;
  }

  public static Evento Toque(int x,  int y) {
    Evento evento = new Evento(TOQUE);
    evento.x_ = x;
    evento.y_ = y;
    return evento;
  }

  public static Evento Detalhamento(int x,  int y) {
    Evento evento = new Evento(DETALHAMENTO);
    evento.x_ = x;
    evento.y_ = y;
    return evento;
  }

  private Evento(int tipo) {
    tipo_ = tipo;
  }

  public String toString() {
    return "Tipo: " + tipoString() + ", escala: " + escala_ + ", rotacao: " + rotacao_ +
                                     ", x:" + x_ + ", y: " + y_ + ", nx: " + nx_ + ", ny: " + ny_;
  }

  private String tipoString() {
    switch (tipo_) {
      case CLIQUE: return "CLIQUE";
      case CLIQUE_DUPLO: return "CLIQUE_DUPLO";
      case ESCALA: return "ESCALA";
      case TOQUE: return "TOQUE";
      case CARREGAR: return "CARREGAR";
      case DETALHAMENTO: return "DETALHAMENTO";
      case PRESSIONADO: return "PRESSIONADO";
      case LIBERADO: return "LIBERADO";
      case MOVIMENTO: return "MOVIMENTO";
      case ROTACAO: return "ROTACAO";
      case TRANSLACAO: return "TRANSLACAO";
      default: return "INVALIDO";
    }
  }

  public int tipo() { return tipo_; }
  public float escala() { return escala_; }
  public float rotacao() { return rotacao_; }
  public int x() { return x_; }
  public int y() { return y_; }
  public int nx() { return nx_; }
  public int ny() { return ny_; }

  private int tipo_;
  private float escala_;
  private float rotacao_;
  private int x_;
  private int y_;
  private int nx_;
  private int ny_;
}
