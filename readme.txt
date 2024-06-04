1. Introducere

Rezolvarea temei este realizata in urmatoarele fisiere sursa:
- server.c/.h: implementarea serverului sub forma unui broker de date primite de
la clienti UDP
- subscriber.c/.h: implementarea unui client TCP ce primeste, analizeaza si
afiseaza date primite de la server
- utils.c/.h: functii si macro-uri folosite atat de client, cat si de server
- queue.c/.h: o implementare a unei cozi folosita in implementarea serverului

2. Serverul

Serverul tine evidenta clientilor intr-o coada de `struct client` - o structura
ce retine descriptorul asociat conexiunii unui client (campul `sockfd`), ID-ul
clientului (campul `id`) si o coada de topic-uri la care este abonat clientul
(campul `subs`).

Serverul functioneaza prin interogarea continua a unei entitati epoll de
multiplexare I/O, in care sunt inserate descriptorii pentru standard input,
socket-ul pasiv de TCP si clientii UDP si TCP. S-a ales folosirea API-ului epoll
datorita comoditatii de a asocia unui descriptor o instanta a unei structuri de
date (in cazul de fata, `struct client`), care este ulterior intoarsa cand
epoll-ul sesizeaza activitate pe descriptorul respectiv.

In cazul activitatii pe descriptorul pentru standard input, se compara intrarea
cu keyword-ul `exit` - singura comanda acceptata de server de la standard input.

In cazul activitatii pe descriptorul pasiv TCP, se accepta cererea de conexiune
sesizata si se insereaza noul descriptor in epoll. Receptionarea ID-ului este
detaliata in paragraful urmator.

In cazul activitatii pe un descriptor al unui client TCP, se verifica initial
daca descriptorul are asociata o instanta a unui `struct client` in epoll. In
caz negativ, conexiunea este noua, i.e inca nu a fost primit ID-ul clientului
asociat conexiuni. Deci, se primeste ID-ul acestuia si se asociaza
descriptorului o noua instanta de `struct client` sau o instanta deja existenta
care contine acelasi ID, dar care nu are descriptor (i.e. clientul nu este
conectat la server), unde se poate. In caz afirmativ, clientul si-a trimis
anterior ID-ul, deci se face o cerere de abonare/dezabonare de la un topic.
Cererea este analizata, iar coada `subs` din instanta de `struct client`
asociata conexiunii este actualizata corespunzator.

In cazul activitatii pe un descriptor al unui client UDP, se receptioneaza
mesajul transmis de acesta si este trimis mai departe clientilor TCP care sunt
abonati la topicul mesajului, impreuna cu adresa clientului UDP si portul pe
care a fost trimis mesajul catre server.

3. Clientul TCP

Clientul TCP functioneaza prin interogarea continua a unei entitati epoll, in
care sunt inserate descriptorii pentru standard input si socket-ul conexiunii
la server. Imediat dupa conectare, clientul trimite ID-ul sau serverului. Daca
serverul sesizeaza ca ID-ul nu e valid, acesta inchide conexiunea cu clientul.

In cazul activitatii pe descriptorul pentru standard input, se compara intrarea
cu keyword-urile `exit`, `subscribe` si `unsubscribe`. Pentru comenzile
`subscribe` si `unsubscribe`, se construieste si trimite serverului o cerere
corespunzatoare cu topic-ul cerut.

In cazul activitatii pe descriptorul pentru conexiunea la server, se primesc
datele trimise de acesta si sunt parsate si afisate la standard output conform
cerintei.

4. Incadrarea mesajelor

Trimiterea datelor intre server si client TCP este intotdeauna precedata de
trimiterea unui header de dimensiune fixa (`struct request_hdr` pentru cereri de
la client la server si `struct message_hdr` pentru mesaje de la server la
client) care contine, printre altele, numarul de octeti al acestor date. Astfel,
receptorul va stii intotdeauna cate date trebuie sa primeasca si poate, prin
urmare, sa incadreze corect mesajele.
