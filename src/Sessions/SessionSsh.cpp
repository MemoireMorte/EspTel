#include "SessionSsh.h"

// libssh_esp32.h d'abord : c'est lui qui fait trouver la bibliothèque par l'IDE Arduino.
#include <libssh_esp32.h>
#include <libssh/libssh.h>

namespace ses {

void sshInitialiser() {
  static bool fait = false;
  if (!fait) {
    libssh_begin();  // ssh_init(), une seule fois : jamais de ssh_finalize() ensuite
    fait = true;
  }
}

bool SessionSsh::transport(const Cible& cible) {
  fermer();
  _erreur[0] = '\0';
  _typeCle[0] = '\0';
  _empreinte[0] = '\0';

  _session = ssh_new();
  if (_session == nullptr) {
    definirErreur("mémoire insuffisante (ssh_new)");
    return false;
  }
  const int port = cible.portEffectif();
  const long delai = long((cible.delaiConnexionMs + 999) / 1000);
  const int verbosite = SSH_LOG_NOLOG;
  ssh_options_set(_session, SSH_OPTIONS_HOST, cible.hote.c_str());
  ssh_options_set(_session, SSH_OPTIONS_PORT, &port);
  ssh_options_set(_session, SSH_OPTIONS_USER, cible.utilisateur.c_str());
  ssh_options_set(_session, SSH_OPTIONS_TIMEOUT, &delai);
  ssh_options_set(_session, SSH_OPTIONS_LOG_VERBOSITY, &verbosite);

  if (ssh_connect(_session) != SSH_OK) {
    definirErreur("%s", ssh_get_error(_session));
    fermer();
    return false;
  }
  releverEmpreinte();
  return true;
}

bool SessionSsh::ouvrirShell(const Cible& cible) {
  if (_session == nullptr) return false;
  _canal = ssh_channel_new(_session);
  if (_canal == nullptr || ssh_channel_open_session(_canal) != SSH_OK) {
    definirErreur("ouverture du canal : %s", ssh_get_error(_session));
    fermer();
    return false;
  }
  if (ssh_channel_request_pty_size(_canal, "vt100", cible.colonnes, cible.lignes) != SSH_OK ||
      ssh_channel_request_shell(_canal) != SSH_OK) {
    definirErreur("pty/shell : %s", ssh_get_error(_session));
    fermer();
    return false;
  }
  return true;
}

void SessionSsh::releverEmpreinte() {
  ssh_key cle = nullptr;
  if (ssh_get_server_publickey(_session, &cle) != SSH_OK) return;
  snprintf(_typeCle, sizeof(_typeCle), "%s", ssh_key_type_to_char(ssh_key_type(cle)));
  unsigned char* hash = nullptr;
  size_t longueur = 0;
  if (ssh_get_publickey_hash(cle, SSH_PUBLICKEY_HASH_SHA256, &hash, &longueur) == 0) {
    char* texte = ssh_get_fingerprint_hash(SSH_PUBLICKEY_HASH_SHA256, hash, longueur);
    if (texte != nullptr) {
      snprintf(_empreinte, sizeof(_empreinte), "%s", texte);
      ssh_string_free_char(texte);
    }
    ssh_clean_pubkey_hash(&hash);
  }
  ssh_key_free(cle);
}

SessionAuth::Auth SessionSsh::authentifier(const Cible& cible) {
  if (_session == nullptr) return Auth::Erreur;
  int rc = ssh_userauth_password(_session, nullptr, cible.motDePasse.c_str());
  if (rc == SSH_AUTH_SUCCESS) return Auth::Ok;

  // Beaucoup de serveurs (PAM) n'acceptent que keyboard-interactive : on répond
  // le mot de passe à chaque question.
  if (rc != SSH_AUTH_ERROR) {
    rc = ssh_userauth_kbdint(_session, nullptr, nullptr);
    for (int tour = 0; rc == SSH_AUTH_INFO && tour < 4; ++tour) {
      const int n = ssh_userauth_kbdint_getnprompts(_session);
      for (int i = 0; i < n; ++i) {
        ssh_userauth_kbdint_setanswer(_session, i, cible.motDePasse.c_str());
      }
      rc = ssh_userauth_kbdint(_session, nullptr, nullptr);
    }
  }
  if (rc == SSH_AUTH_SUCCESS) return Auth::Ok;
  if (rc == SSH_AUTH_ERROR || ssh_is_connected(_session) == 0) {
    definirErreur("connexion perdue pendant l'identification");
    return Auth::Erreur;
  }
  definirErreur("mot de passe refusé pour %s", cible.utilisateur.c_str());
  return Auth::Refusee;
}

SessionAuth::Auth SessionSsh::authentifierCle(const Cible& cible) {
  if (_session == nullptr) return Auth::Erreur;
  ssh_key cle = nullptr;
  if (ssh_pki_import_privkey_base64(cible.clePrivee.c_str(), nullptr, nullptr, nullptr, &cle) != SSH_OK) {
    definirErreur("clé privée illisible");
    return Auth::Refusee;
  }
  const int rc = ssh_userauth_publickey(_session, nullptr, cle);
  ssh_key_free(cle);
  if (rc == SSH_AUTH_SUCCESS) return Auth::Ok;
  if (rc == SSH_AUTH_ERROR || ssh_is_connected(_session) == 0) {
    definirErreur("connexion perdue pendant l'identification");
    return Auth::Erreur;
  }
  definirErreur("clé refusée pour %s", cible.utilisateur.c_str());
  return Auth::Refusee;
}

int SessionSsh::lire(uint8_t* buf, size_t taille) {
  if (_canal == nullptr) return -1;
  const int n = ssh_channel_read_nonblocking(_canal, buf, taille, 0);
  if (n > 0) return n;
  if (n < 0 || ssh_channel_is_eof(_canal) || !ssh_channel_is_open(_canal)) {
    if (n == SSH_ERROR) definirErreur("%s", ssh_get_error(_session));
    else definirErreur("fermée par le serveur");
    return -1;
  }
  return 0;
}

bool SessionSsh::envoyer(const uint8_t* donnees, size_t n) {
  if (_canal == nullptr) return false;
  return ssh_channel_write(_canal, donnees, n) != SSH_ERROR;
}

void SessionSsh::fermer() {
  if (_canal != nullptr) {
    if (ssh_channel_is_open(_canal)) {
      ssh_channel_send_eof(_canal);
      ssh_channel_close(_canal);
    }
    ssh_channel_free(_canal);
    _canal = nullptr;
  }
  if (_session != nullptr) {
    ssh_disconnect(_session);
    ssh_free(_session);
    _session = nullptr;
  }
}

}  // namespace ses
