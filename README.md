### Swarm Adaptive Network

**Motivați alegerea bibliotecilor folosite în cadrul proiectului**

-   ESP8266HTTPClient - pentru a putea face request-uri HTTP către serverul de bază
-   ESP8266WiFi - pentru a putea conecta placa la un router WiFi
-   ESP8266WebServer - pentru a putea crea un server, pentru setup-ul inițial al plăcii
-   U8g2lib - pentru a putea controla display-ul OLED, mai eficientă decât librăria Adafruit din punct de vedere al gestionării memoriei
-   ESP8266mDNS și ArduinOTA - pentru a putea face update-uri Over The Air
-   espnow - pentru a putea comunica între plăci fără a avea nevoie de un router WiFi
-   ArduinoJson - pentru a putea manipula JSON-uri

**Evidențiați elementul de noutate al proiectului**

Comunicarea între un Network de plăci fără a avea nevoie de un router WiFi, folosind ESP-NOW. (Exemplu de utilizare: sistem de monitorizare a unui câmp de panouri solare)
Esp-now are o distanță de acoperire de 3 ori mai mare ca WiFi-ul.

Utilizarea foarte simplă a unui sistem de 100 de dispozitive fără a fi nevoie de configurare manuală a fiecărei plăci.

**Justificați utilizarea funcționalităților din laborator în cadrul proiectului.**

Am folosit noțiunile din laborator mai mult ca și o bază, proiectul fiind axat foarte mult pe partea de software.

**Explicați scheletul proiectului, interacțiunea dintre funcționalități și modul în care a fost validat că acestea funcționează conform**

Scheletul implementează un cod universal care se află pe fiecare placă, plăcile pot intra în mai multe moduri pentru a asigura o extindere și o asigurare a transmiterii informației.

**Explicați cum, de ce și unde ați realizat optimizări**

Am realizat optimizări în ceea ce privește consumul de energie, am folosit ESP-NOW pentru a comunica între plăci, fără a avea nevoie de un router WiFi, astfel consumul de energie este mult mai mic.
Am încercat ca nodurile finale să intre în sleep mode cât mai des posibil, pentru a economisi cât mai multă energie.
# Swarm-Adaptive-Network
