/** @file not/Notificacao.cpp implementacao das funcoes basicas de notificacao. */

#include "ntf/notificacao.h"
#include "ntf/notificacao.pb.h"

using namespace ntf;

CentralNotificacoes::CentralNotificacoes() {}
CentralNotificacoes::~CentralNotificacoes() {
  for (auto* n : notificacoes_) {
    delete n;
  }
}

void CentralNotificacoes::RegistraReceptor(Receptor* receptor) {
  receptores_.push_back(receptor);
}

void CentralNotificacoes::RegistraReceptorRemoto(ReceptorRemoto* receptor) {
  receptores_remotos_.push_back(receptor);
}

void CentralNotificacoes::DesregistraReceptor(const Receptor* receptor) {
  for (auto it = receptores_.begin(); it != receptores_.end(); ++it) {
    if (*it == receptor) {
      receptores_.erase(it);
      break;
    }
  }
}

void CentralNotificacoes::DesregistraReceptorRemoto(const ReceptorRemoto* receptor) {
  for (auto it = receptores_remotos_.begin(); it != receptores_remotos_.end(); ++it) {
    if (*it == receptor) {
      receptores_remotos_.erase(it);
      break;
    }
  }
}

#include <iosfwd>
#include <iostream>

void CentralNotificacoes::AdicionaNotificacao(Notificacao* notificacao) {
  //std::cout << "Adicionando: " << notificacao->ShortDebugString() << std::endl;
  notificacoes_.push_back(notificacao);
}

void CentralNotificacoes::Notifica() {
  // Realiza a copia pq pode haver novas notificacoes durante o loop.
  std::vector<Notificacao*> copia_notificacoes;
  copia_notificacoes.swap(notificacoes_);
  for (auto* n : copia_notificacoes) {
    if (n->tipo() != ntf::TN_TEMPORIZADOR) {
      std::cout << "Despachando: " << n->ShortDebugString() << std::endl;
    }
    if (n->local()) {
      for (auto* r : receptores_) {
        r->TrataNotificacao(*n);
      }
    }
    if (n->remota()) {
      for (auto* r : receptores_remotos_) {
        r->TrataNotificacaoRemota(*n);
      }
    }
    delete n;
  }
}
