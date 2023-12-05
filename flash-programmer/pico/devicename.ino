#define NICKNAME_PATH "._nick"

const char *makeNicknamedDeviceName() {
  static char deviceStr[64];

  const char *nick = getNickname();
  if (nick) {
    sprintf(deviceStr, "%s's Floopy Drive", nick);
  } else {
    sprintf(deviceStr, "Floopy Drive");
  }

  return deviceStr;
}

const char *getNickname() {
  File f = LittleFS.open(NICKNAME_PATH, "r");
  if (f) {
    String nick = f.readString();
    f.close();
    if (nick && nick.length() > 0) {
      return nick.c_str();
    }
  }
  return NULL;
}

void setNickname(const char *nick) {
  if (nick && strlen(nick) > 0) {
    File f = LittleFS.open(NICKNAME_PATH, "w");
    if (!f) return;
    if (strlen(nick) <= 48) {
      f.write(nick);
      f.flush();
      echo_all("Nickname set\r\n");
    } else {
      echo_all("!ERR Nickname too long\r\n");
    }
    f.close();
  } else {
    LittleFS.remove(NICKNAME_PATH);
    echo_all("Nickname cleared\r\n");
  }
}
