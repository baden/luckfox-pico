
  # Треба створити скрипт, який буде піднімати Wireguard

На сервері, у файлі конфігурації /etc/wireguard/wg0.conf
треба взяти приватний ключ сервера, і з нього згенерувати публічний ключ
через `wg pubkey`. Воно повинно працювати через `echo "$SERVER_PRIV_KEY" | wg pubkey` але мені чомусь повертало порожню строку.
Я зберіг приватний ключ у файл `privatekey` та виконав `wg pubkey < privatekey > publickey`.

На Luckfox треба створити приватний та публічний ключі.

```bash
cd /etc/wireguard/
umask 077
wg genkey > privatekey
wg pubkey < privatekey > publickey
```

Взяти публічний ключ з Luckfox, та прописати його в налаштування для Peer

```
...
[Peer]
# Dron (Luckfox)
PublicKey = <сюди>
...
```

На Luckfox треба створити скрипт для підняття VPN при старті:

```
ip link add dev wg0 type wireguard
ip address add dev wg0 10.8.0.2/24
wg set wg0 listen-port 51820 private-key /etc/wireguard/privatekey peer <публічний ключ з сервера> allowed-ips 10.8.0.0/24 endpoint <ip сервера>:51820
ip link set up dev wg0
```

Подивитись стан можна через

```
wg show
ping 10.8.0.2
ping 10.8.0.1
```
