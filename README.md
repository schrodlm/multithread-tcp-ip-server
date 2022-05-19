# TCP/IP server
Server na navádění robotů (klientů) k cíli.
Tento projekt byl vytvářen jako semstrální práce k předmětu B-PSI ( Počítačové sítě ) na univerzitě FIT ČVUT.

Cílem úlohy bylo vytvořit vícevláknový server pro TCP/IP komunikaci a implementovat komunikační protokol podle dané specifikace. 

Implementace klientské části není součástí úlohy!

# Zadání

Vytvořte server pro automatické řízení vzdálených robotů. Roboti se sami přihlašují k serveru a ten je navádí ke středu souřadnicového systému. Pro účely testování každý robot startuje na náhodných souřadnicích a snaží se dojít na souřadnici [0,0]. Na cílové souřadnici musí robot vyzvednout tajemství. Po cestě k cíli mohou roboti narazit na překážky, které musí obejít. Server zvládne navigovat více robotů najednou a implementuje bezchybně komunikační protokol.

## Detailní specifikace

Komunikace mezi serverem a roboty je realizována plně textovým protokolem. Každý příkaz je zakončen dvojicí speciálních symbolů „\\a\\b“. (Jsou to **dva** znaky '\\a' a '\\b'.) Server musí dodržet komunikační protokol do detailu přesně, ale musí počítat s nedokonalými firmwary robotů (viz sekce Speciální situace).

Zprávy serveru:

<table><colgroup><col><col><col></colgroup><tbody><tr><th>Název</th><th>Zpráva</th><th>Popis</th></tr><tr><td>SERVER_CONFIRMATION</td><td>&lt;16-bitové číslo v decimální notaci&gt;\a\b</td><td>Zpráva s potvrzovacím kódem. Může obsahovat maximálně 5 čísel a ukončovací sekvenci \a\b.</td></tr><tr><td>SERVER_MOVE</td><td>102 MOVE\a\b</td><td>Příkaz pro pohyb o jedno pole vpřed</td></tr><tr><td>SERVER_TURN_LEFT</td><td>103 TURN LEFT\a\b</td><td>Příkaz pro otočení doleva</td></tr><tr><td>SERVER_TURN_RIGHT</td><td>104 TURN RIGHT\a\b</td><td>Příkaz pro otočení doprava</td></tr><tr><td>SERVER_PICK_UP</td><td>105 GET MESSAGE\a\b</td><td>Příkaz pro vyzvednutí zprávy</td></tr><tr><td>SERVER_LOGOUT</td><td>106 LOGOUT\a\b</td><td>Příkaz pro ukončení spojení po úspěšném vyzvednutí zprávy</td></tr><tr><td>SERVER_KEY_REQUEST</td><td>107 KEY REQUEST\a\b</td><td>Žádost serveru o Key ID pro komunikaci</td></tr><tr><td>SERVER_OK</td><td>200 OK\a\b</td><td>Kladné potvrzení</td></tr><tr><td>SERVER_LOGIN_FAILED</td><td>300 LOGIN FAILED\a\b</td><td>Nezdařená autentizace</td></tr><tr><td>SERVER_SYNTAX_ERROR</td><td>301 SYNTAX ERROR\a\b</td><td>Chybná syntaxe zprávy</td></tr><tr><td>SERVER_LOGIC_ERROR</td><td>302 LOGIC ERROR\a\b</td><td>Zpráva odeslaná ve špatné situaci</td></tr><tr><td>SERVER_KEY_OUT_OF_RANGE_ERROR</td><td>303 KEY OUT OF RANGE\a\b</td><td>Key ID není v očekávaném rozsahu</td></tr></tbody></table>

Zprávy klienta:

<table><colgroup><col><col><col><col><col></colgroup><tbody><tr><th>Název</th><th>Zpráva</th><th>Popis</th><th>Ukázka</th><th>Maximální délka</th></tr><tr><td>CLIENT_USERNAME</td><td>&lt;user name&gt;\a\b</td><td>Zpráva s uživatelským jménem. Jméno může být libovolná sekvence znaků kromě kromě dvojice \a\b.</td><td>Umpa_Lumpa\a\b</td><td>20</td></tr><tr><td>CLIENT_KEY_ID</td><td>&lt;Key ID&gt;\a\b</td><td>Zpráva obsahující Key ID. Může obsahovat pouze celé číslo o maximálně třech cifrách.</td><td>2\a\b</td><td>5</td></tr><tr><td>CLIENT_CONFIRMATION</td><td>&lt;16-bitové číslo v decimální notaci&gt;\a\b</td><td>Zpráva s potvrzovacím kódem. Může obsahovat maximálně 5 čísel a ukončovací sekvenci \a\b.</td><td>1009\a\b</td><td>7</td></tr><tr><td>CLIENT_OK</td><td>OK &lt;x&gt; &lt;y&gt;\a\b</td><td>Potvrzení o provedení pohybu, kde <em>x</em> a <em>y</em> jsou souřadnice robota po provedení pohybového příkazu.</td><td>OK -3 -1\a\b</td><td>12</td></tr><tr><td>CLIENT_RECHARGING</td><td>RECHARGING\a\b</td><td>Robot se začal dobíjet a přestal reagovat na zprávy.</td><td></td><td>12</td></tr><tr><td>CLIENT_FULL_POWER</td><td>FULL POWER\a\b</td><td>Robot doplnil energii a opět příjímá příkazy.</td><td></td><td>12</td></tr><tr><td>CLIENT_MESSAGE</td><td>&lt;text&gt;\a\b</td><td>Text vyzvednutého tajného vzkazu. Může obsahovat jakékoliv znaky kromě ukončovací sekvence \a\b.</td><td>Haf!\a\b</td><td>100</td></tr></tbody></table>

Časové konstanty:

<table><colgroup><col><col><col></colgroup><tbody><tr><th>Název</th><th>Hodnota [s]</th><th>Popis</th></tr><tr><td>TIMEOUT</td><td>1</td><td>Server i klient očekávají od protistrany odpověď po dobu tohoto intervalu.</td></tr><tr><td>TIMEOUT_RECHARGING</td><td>5</td><td>Časový interval, během kterého musí robot dokončit dobíjení.</td></tr></tbody></table>

Komunikaci s roboty lze rozdělit do několika fází:

### Autentizace

Server i klient oba znají pět dvojic autentizačních klíčů (nejedná se o veřejný a soukromý klíč):

<table><colgroup><col><col><col></colgroup><tbody><tr><th>Key ID</th><th>Server Key</th><th>Client Key</th></tr><tr><td>0</td><td>23019</td><td>32037</td></tr><tr><td>1</td><td>32037</td><td>29295</td></tr><tr><td>2</td><td>18789</td><td>13603</td></tr><tr><td>3</td><td>16443</td><td>29533</td></tr><tr><td>4</td><td>18189</td><td>21952</td></tr></tbody></table>

Každý robot začne komunikaci odesláním svého uživatelského jména (zpráva CLIENT\_USERNAME). Uživatelské jméno múže být libovolná sekvence 18 znaků neobsahující sekvenci „\\a\\b“. V dalším kroku vyzve server klienta k odeslání Key ID (zpráva SERVER\_KEY\_REQUEST), což je vlastně identifikátor dvojice klíčů, které chce použít pro autentizaci. Klient odpoví zprávou CLIENT\_KEY\_ID, ve které odešle Key ID. Po té server zná správnou dvojici klíčů, takže může spočítat "hash" kód z uživatelského jména podle následujícího vzorce:

```
Uživatelské jméno: Mnau!

ASCII reprezentace: 77 110 97 117 33

Výsledný hash: ((77 + 110 + 97 + 117 + 33) * 1000) % 65536 = 40784
```

Výsledný hash je 16-bitové číslo v decimální podobě. Server poté k hashi přičte klíč serveru tak, že pokud dojde k překročení kapacity 16-bitů, hodnota jednoduše přetečení:

```
(40784 + 54621) % 65536 = 29869
```

Výsledný potvrzovací kód serveru se jako text pošle klientovi ve zprávě SERVER\_CONFIRM. Klient z obdrženého kódu vypočítá zpátky hash a porovná ho s očekávaným hashem, který si sám spočítal z uživatelského jména. Pokud se shodují, vytvoří potvrzovací kód klienta. Výpočet potvrzovacího kódu klienta je obdobný jako u serveru, jen se použije klíč klienta:

```
(40784 + 45328) % 65536 = 20576
```

Potvrzovací kód klienta se odešle serveru ve zpráve CLIENT\_CONFIRMATION, který z něj vypočítá zpátky hash a porovná jej s původním hashem uživatelského jména. Pokud se obě hodnoty shodují, odešle zprávu SERVER\_OK, v opačném prípadě reaguje zprávou SERVER\_LOGIN\_FAILED a ukončí spojení. Celá sekvence je na následujícím obrázku:

```
Klient                  Server
------------------------------------------
CLIENT_USERNAME     --->
                    <---    SERVER_KEY_REQUEST
CLIENT_KEY_ID       --->
                    <---    SERVER_CONFIRMATION
CLIENT_CONFIRMATION --->
                    <---    SERVER_OK
                              nebo
                            SERVER_LOGIN_FAILED
                      .
                      .
                      .
```

Server dopředu nezná uživatelská jména. Roboti si proto mohou zvolit jakékoliv jméno, ale musí znát sadu klíčů klienta i serveru. Dvojice klíčů zajistí oboustranou autentizaci a zároveň zabrání, aby byl autentizační proces kompromitován prostým odposlechem komunikace.

### Pohyb robota k cíli

Robot se může pohybovat pouze rovně (SERVER\_MOVE) a je schopen provést otočení na místě doprava (SERVER\_TURN\_RIGHT) i doleva (SERVER\_TURN\_LEFT). Po každém příkazu k pohybu odešle potvrzení (CLIENT\_OK), jehož součástí je i aktuální souřadnice. Pozice robota není serveru na začátku komunikace známa. Server musí zjistit polohu robota (pozici a směr) pouze z jeho odpovědí. Z důvodů prevence proti nekonečnému bloudění robota v prostoru, má každý robot omezený počet pohybů (pouze posunutí vpřed). Počet pohybů by měl být dostatečný pro rozumný přesun robota k cíli. Následuje ukázka komunkace. Server nejdříve pohne dvakrát robotem kupředu, aby detekoval jeho aktuální stav a po té jej navádí směrem k cílové souřadnici \[0,0\].

```
Klient                  Server
------------------------------------------
                  .
                  .
                  .
                <---    SERVER_MOVE
                          nebo
                        SERVER_TURN_LEFT
                          nebo
                        SERVER_TURN_RIGHT
CLIENT_OK       --->
                <---    SERVER_MOVE
                          nebo
                        SERVER_TURN_LEFT
                          nebo
                        SERVER_TURN_RIGHT
CLIENT_OK       --->
                <---    SERVER_MOVE
                          nebo
                        SERVER_TURN_LEFT
                          nebo
                        SERVER_TURN_RIGHT
                  .
                  .
                  .
```

Těsně po autentizaci robot očekává alespoň jeden pohybový příkaz - SERVER\_MOVE, SERVER\_TURN\_LEFT nebo SERVER\_TURN\_RIGHT! Nelze rovnou zkoušet vyzvednout tajemství. Po cestě k cíli se nachází mnoho překážek, které musí roboti překonat objížďkou. Pro překážky platí následující pravidla:

-   Překážka okupuje vždy jedinou souřadnici.
-   Je zaručeno, že každá překážka má prázdné všechny sousední souřadnice (tedy vždy lze jednoduše objet).
-   Je zaručeno, že překážka nikdy neokupuje souřadnici \[0,0\].
-   Pokud robot narazí do překážky více než dvacetkrát, poškodí se a ukončí spojení.

Překážka je detekována tak, že robot dostane pokyn pro pohyb vpřed (SERVER\_MOVE), ale nedojde ke změně souřadnic (zpráva CLIENT\_OK obsahuje stejné souřadnice jako v předchozím kroku). Pokud se pohyb neprovede, nedojde k odečtení z počtu zbývajících kroků robota.

### Vyzvednutí tajného vzkazu

Poté, co robot dosáhne cílové souřadnice \[0,0\], tak se pokusí vyzvednout tajný vzkaz (zpráva SERVER\_PICK\_UP). Pokud je robot požádán o vyzvednutí vzkazu a nenachází se na cílové souřadnici, spustí se autodestrukce robota a komunikace se serverem je přerušena. Při pokusu o vyzvednutí na cílově souřadnici reaguje robot zprávou CLIENT\_MESSAGE. Server musí na tuto zprávu zareagovat zprávou SERVER\_LOGOUT. (Je zaručeno, že tajný vzkaz se nikdy neshoduje se zprávou CLIENT\_RECHARGING, pokud je tato zpráva serverem obdržena po žádosti o vyzvednutí jedná se vždy o dobíjení.) Poté klient i server ukončí spojení. Ukázka komunikace s vyzvednutím vzkazu:

```
Klient                  Server
------------------------------------------
                  .
                  .
                  .
                <---    SERVER_PICK_UP
CLIENT_MESSAGE  --->
                <---    SERVER_LOGOUT
```


## Chybové situace

Někteří roboti mohou mít poškozený firmware a tak mohou komunikovat špatně. Server by měl toto nevhodné chování detekovat a správně zareagovat.

### Chyby při autentizaci

Pokud je ve zprávě CLIENT\_KEY\_ID Key ID, který je mimo očekávaný rozsah (tedy číslo, které není mezi 0-4), tak na to server reaguje chybovou zprávou SERVER\_KEY\_OUT\_OF\_RANGE\_ERROR a ukončí spojení. Za číslo se pro zjednodušení považují i záporné hodnoty. Pokud ve zprávě CLIENT\_KEY\_ID není číslo (např. písmena), tak na to server reaguje chybou SERVER\_SYNTAX\_ERROR.

Pokud je ve zprávě CLIENT\_CONFIRMATION číselná hodnota (i záporné číslo), která neodpovídá očekávanému potvrzení, tak server pošle zprávu SERVER\_LOGIN\_FAILED a ukončí spojení. Pokud se nejedná vůbec o čistě číselnou hodnotu, tak server pošle zprávu SERVER\_SYNTAX\_ERROR a ukončí spojení.

### Syntaktická chyba

Na syntaktickou chybu reagauje server vždy okamžitě po obdržení zprávy, ve které chybu detekoval. Server pošle robotovi zprávu SERVER\_SYNTAX\_ERROR a pak musí co nejdříve ukončit spojení. Syntakticky nekorektní zprávy:

-   Příchozí zpráva je delší než počet znaků definovaný pro každou zprávu (včetně ukončovacích znaků \\a\\b). Délky zpráv jsou definovány v tabulce s přehledem zpráv od klienta.
-   Příchozí zpráva syntakticky neodpovídá ani jedné ze zpráv CLIENT\_USERNAME, CLIENT\_KEY\_ID, CLIENT\_CONFIRMATION, CLIENT\_OK, CLIENT\_RECHARGING a CLIENT\_FULL\_POWER.

Každá příchozí zpráva je testována na maximální velikost a pouze zprávy CLIENT\_CONFIRMATION, CLIENT\_OK, CLIENT\_RECHARGING a CLIENT\_FULL\_POWER jsou testovány na jejich obsah (zprávy CLIENT\_USERNAME a CLIENT\_MESSAGE mohou obsahovat cokoliv).

### Logická chyba

Logická chyba nastane pouze při nabíjení - když robot pošle info o dobíjení (CLIENT\_RECHARGING) a po té pošle jakoukoliv jinou zprávu než CLIENT\_FULL\_POWER nebo pokud pošle zprávu CLIENT\_FULL\_POWER, bez předchozího odeslání CLIENT\_RECHARGING. Server na takové situace reaguje odesláním zprávy SERVER\_LOGIC\_ERROR a okamžitým ukončením spojení.

### Timeout

Protokol pro komunikaci s roboty obsahuje dva typy timeoutu:

-   TIMEOUT - timeout pro komunikaci. Pokud robot nebo server neobdrží od své protistrany žádnou komunikaci (nemusí to být však celá zpráva) po dobu tohoto časového intervalu, považují spojení za ztracené a okamžitě ho ukončí.
-   TIMEOUT\_RECHARGING - timeout pro dobíjení robota. Po té, co server přijme zprávu CLIENT\_RECHARGING, musí robot nejpozději do tohoto časového intervalu odeslat zprávu CLIENT\_FULL\_POWER. Pokud to robot nestihne, server musí okamžitě ukončit spojení.

## Speciální situace

Při komunikaci přes komplikovanější síťovou infrastrukturu může docházet ke dvěma situacím:

-   Zpráva může dorazit rozdělena na několik částí, které jsou ze socketu čteny postupně. (K tomu dochází kvůli segmentaci a případnému zdržení některých segmentů při cestě sítí.)
-   Zprávy odeslané brzy po sobě mohou dorazit téměř současně. Při jednom čtení ze socketu mohou být načteny obě najednou. (Tohle se stane, když server nestihne z bufferu načíst první zprávu dříve než dorazí zpráva druhá.)

Za použití přímého spojení mezi serverem a roboty v kombinaci s výkonným hardwarem nemůže k těmto situacím dojít přirozeně, takže jsou testovačem vytvářeny uměle. V některých testech jsou obě situace kombinovány.

Každý správně implementovaný server by se měl umět s touto situací vyrovnat. Firmwary robotů s tímto faktem počítají a dokonce ho rády zneužívají. Pokud se v protokolu vyskytuje situace, kdy mají zprávy od robota předem dané pořadí, jsou v tomto pořadí odeslány najednou. To umožňuje sondám snížit jejich spotřebu a zjednodušuje to implementaci protokolu (z jejich pohledu).

## Optimalizace serveru

Server optimalizuje protokol tak, že nečeká na dokončení zprávy, která je očividně špatná. Například na výzvu k autentizaci pošle robot pouze část zprávy s uživatelským jménem. Server obdrží např. 22 znaků uživatelského jména, ale stále neobdržel ukončovací sekvenci \\a\\b. Vzhledem k tomu, že maximální délka zprávy je 20 znaků, je jasné, že přijímaná zpráva nemůže být validní. Server tedy zareaguje tak, že nečeká na zbytek zprávy, ale pošle zprávu SERVER\_SYNTAX\_ERROR a ukončí spojení. V principu by měl postupovat stejně při vyzvedávání tajného vzkazu.

V případě části komunikace, ve které se robot naviguje k cílovým souřadnicím očekává tři možné zprávy: CLIENT\_OK, CLIENT\_RECHARGING nebo CLIENT\_FULL\_POWER. Pokud server načte část neúplné zprávy a tato část je delší než maximální délka těchto zpráv, pošle SERVER\_SYNTAX\_ERROR a ukončí spojení. Pro pomoc při optimalizaci je u každé zprávy v tabulce uvedena její maximální velikost.

## Ukázka komunikace

```
C: "Oompa Loompa\a\b"
S: "107 KEY REQUEST\a\b"
C: "0\a\b"
S: "64907\a\b"
C: "8389\a\b"
S: "200 OK\a\b"
S: "102 MOVE\a\b"
C: "OK 0 0\a\b"
S: "102 MOVE\a\b"
C: "OK -1 0\a\b"
S: "104 TURN RIGHT\a\b"
C: "OK -1 0\a\b"
S: "104 TURN RIGHT\a\b"
C: "OK -1 0\a\b"
S: "102 MOVE\a\b"
C: "OK 0 0\a\b"
S: "105 GET MESSAGE\a\b"
C: "Tajny vzkaz.\a\b"
S: "106 LOGOUT\a\b"
```





