#undef UNICODE

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <locale.h>
#include <thread>
#include <iostream>
#include <string>
#include <cctype>
#include <vector>
#include <semaphore>
#include <mutex>
#include <condition_variable>

// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")
// #pragma comment (lib, "Mswsock.lib")

std::string slowo;
std::string visualization_slowo;

int liczba_klientow = 0;   //liczba aktualnych klientów
int IleGraczy; //ile graczy finalnie bierze udzia³ w grze
int** wyniki = nullptr;

bool IsGameStarted = false;

SOCKET* KLIENCI = new SOCKET[liczba_klientow];  //dynamiczna tablica dla socket klientów

#define DEFAULT_BUFLEN 36
#define DEFAULT_PORT "27015"

std::mutex graczeMTX;
std::condition_variable graczeSEM;

void Powiadom(int id, int kom)    //funkcja bedzie wysy³a³a do ka¿dego wiadomoœæ o wzorze id ' ' 0/1/2/3
{
    std::string t = std::to_string(id);

    char c[35];
    
    int position = 0;

    for (int i = 0; i < t.length(); i++)
    {
        c[i] = NULL;
        c[i] = t[i];
        position++;
    }
    c[position] = ' ';
    position++;
    c[position] = kom;
    
    for (int i = 0; i < liczba_klientow; i++)     //przesylanie do klientow
    {
        send(KLIENCI[i], c, position+1, 0);
    }
    std::cout << "Wyslano: " << c[0] << " " << int(c[2]) << std::endl;
}

void DisqualificationWyniki(int id)   //gdy gracz wyjdzie w trakcie gry - dajemy mu po prostu -1 punktów i zerujemy ¿ycia (liczba -1 punktów, ma sugerowaæ, ze wyszedl)
{
    for (int i = 0; i < liczba_klientow; i++)
    {
        if (wyniki[i][0] == id)
        {
            wyniki[i][2] = -1;
            wyniki[i][1] = 0;
            break;
        }
    }
}

void BiggerWyniki()
{
    int** nowa_tablica = new int* [liczba_klientow + 1];
    for (int i = 0; i < liczba_klientow; ++i) {
        nowa_tablica[i] = wyniki[i];
    }
    nowa_tablica[liczba_klientow] = new int[3];
    for (int j = 0; j < 3; ++j) {
        nowa_tablica[liczba_klientow][j] = NULL;
    }
    delete[] wyniki;
    wyniki = nowa_tablica;
}

void BreakConnection(SOCKET ClientSocket)
{
    std::cout << "zerwano polaczenie!" << std::endl;
    closesocket(ClientSocket);
    for (int i = 0; i < liczba_klientow; i++)
    {
        if (KLIENCI[i] == ClientSocket)
        {
            KLIENCI[i] = NULL;
            for (int x = i; x < liczba_klientow - 1; x++)
            {
                KLIENCI[x] = KLIENCI[x + 1];
            }
            break;
        }
    }
    liczba_klientow--;
}

void GenNewWord() // generuje nowe slowo    //problem z generowaniem s³owa!
{
    std::vector<std::string> slowa = { "kot", "pies", "dom", "rower", "komputer", "dach", "siedem", "krowa", "owca", "procesor", "schab", "cyfra" };   //slowa

    visualization_slowo.clear();

    for (int i = 0; i < slowo.length(); i++)   //wizualizacja
    {
        visualization_slowo[i] = NULL;
    }

    slowo = slowa[rand() % slowa.size()];  //losuj slowo

    //std::cout << slowo << std::endl;   //wypisz slowo (kontrola)
    visualization_slowo = slowo;

    for (int i = 0; i < slowo.length(); i++)   //wizualizacja
    {
        visualization_slowo[i] = '_';
    }
    //std::cout << visualization_slowo << std::endl;
}

void gra(SOCKET ClientSocket, int id)
{
    std::cout << "Moje id to: " << id << std::endl;
    char recvbuf[DEFAULT_BUFLEN];
    int recvbuflen = DEFAULT_BUFLEN;
    int GetFromClient;
    bool CanPlay = true;

    do
    {
        GetFromClient = recv(ClientSocket, recvbuf, recvbuflen, 0);
        if (GetFromClient > 0) {
            //printf("Bytes received: %d\n", GetFromClient);
            //printf(recvbuf);
            //printf("\n");
            if (recvbuf[0] == 'A')
            {
                std::cout << "Klient zaakceptowa³ gre!" << std::endl;
                break;
            }
        }
        else     //jesli sie rozlaczy to jest usuwany z tablicy
        {
            int new_table_pos = 0;
            int** nowa_tablica = new int* [liczba_klientow - 1];
            for (int i = 0; i < liczba_klientow; ++i) 
            {
                if (id != wyniki[i][0])
                {
                    nowa_tablica[new_table_pos] = wyniki[i];
                    new_table_pos++;
                }
            }
            delete[] wyniki;
            wyniki = nowa_tablica;
            BreakConnection(ClientSocket);
            return;
        }

    } while (true);
    
    if (IsGameStarted == 1)
    {
        //usuwanie delikwenta z listy
        CanPlay = false;
        std::cout << "obserwer" << std::endl;
    }

    do
    {
        std::unique_lock<std::mutex> graczelck(graczeMTX);
        graczeSEM.wait(graczelck, [] {return liczba_klientow >= 2; });

        for (int i = 0; i < 10; i++)   //czekamy okreslona ilosc czasu chyba, ze liczba klientow spadnie do 1 lub osiagnie max CHANGE TO 60
        {
            Sleep(1000);    //Co sekundê sprawdzamy, czy ktoœ siê nie roz³¹czy³
            char check[1];
            check[0] = '0';       //Wysy³amy 0; jeœli nie dojdzie (CheckClient zwróci wartoœæ <0, czyli error, wtedy wiadomo, ¿e ktoœ siê roz³¹czy³ i trzeba zmodyfikowaæ tabele, zamkn¹æ gniazdo etc)
            int CheckClient = send(ClientSocket, check, 1, 0);
            if (CheckClient < 0)
            {
                //usun uzytkownika
                for (int x = 0; x < liczba_klientow; x++)
                {
                    if (wyniki[x][0] == id)
                    {
                        for (int y = x; y < liczba_klientow-1; y++)
                        {
                            wyniki[y][0] = wyniki[y+1][0];
                            wyniki[y][1] = wyniki[y+1][1];
                            wyniki[y][2] = wyniki[y+1][2];
                        }
                        break;
                    }
                }
                BreakConnection(ClientSocket);
                std::cout << "Ktos sie rozlaczyl!" << std::endl;
                return;
            }
            for (int i = 0; i < sizeof(recvbuf); i++)  //zamiana duzych liter na male (kompatybilnosc)
            {
                recvbuf[i] = std::tolower(recvbuf[i]);
            }

            if (liczba_klientow < 2)
            {
                break;
            }
            else if (IsGameStarted == true)
            {
                break;
            }
        }

    } while (liczba_klientow < 2);

    IsGameStarted = true; //zablokuj, by nie da³o sie przeslac nic do czasu rozpoczecia gry
    std::cout << "Rozpoczela sie gra!" << std::endl;
    IleGraczy = liczba_klientow;

    char check[1];
    check[0] = '2';
    std::cout << check[0] << std::endl;
    int CheckClient = send(ClientSocket, check, 1, 0);    //wyslanie powiadomienia o rozpoczaniu gry
    
    //przeslanie tabeli (ile graczy + jakie indeksy). Tabela zawsze wyglada nastepujaco: 1 liczba - ilosc graczy; 2 liczba informacja, czy jestes graczem (0) czy obserwatorem (1); 3  twoj indeks; pozostale liczby - pozostale indeksy. Wszystko to jest oddzielone ' '.
    std::string info;
    std::string help1;
    help1.clear();
    help1 = std::to_string(IleGraczy);
    for (int i = 0; i < help1.length(); i++)
    {   
        info.push_back(help1[i]);
    }
    info.push_back(' ');
    
    if (CanPlay == true)
    {
        info.push_back('0');
    }
    else
    {
        info.push_back('1');
    }
    info.push_back(' ');

    std::string help2;
    help2.clear();
    help2 = std::to_string(id);
    for (int i = 0; i < help2.length(); i++)
    {
        info.push_back(help2[i]);
    }
    info.push_back(' ');

    for (int i = 0; i < IleGraczy; i++)
    {
        if (wyniki[i][0] != id)
        {
            std::string help3;
            help3.clear();
            help3 = std::to_string(wyniki[i][0]);
            for (int a = 0; a < help3.length(); a++)
            {
                info.push_back(help3[a]);
            }
            info.push_back(' ');
        }
    }

    std::cout << "info "<< info.c_str() << std::endl;

    int Gracze = send(ClientSocket, info.c_str(), sizeof(info), 0);   //przesylanie do gracza
    if (Gracze == SOCKET_ERROR)
    {
        std::cout << "Problem Gracze" << std::endl;
        DisqualificationWyniki(id);
        Powiadom(id, 3);
    }

    do    //petla gracza
    {
        int dl = visualization_slowo.length();    //wyslij stan odgadnietego slowa do serwera
        char sendbuf[35];
        for (int i = 0; i < 35; i++)
        {
            sendbuf[i] = NULL;
            if (i < visualization_slowo.length())
            {
                sendbuf[i] = visualization_slowo[i];   //pakujemy slowo w wersji wizualizacja
            }
        }

        for (int i = 0; i < liczba_klientow; i++)   //wysylamy do kazdego
        {
            //std::cout << sendbuf << std::endl;
            int SendToClient = send(KLIENCI[i], sendbuf, visualization_slowo.length(), 0);
            if (SendToClient == SOCKET_ERROR) {
                DisqualificationWyniki(id);
                Powiadom(id, 3);
                BreakConnection(ClientSocket);
                return;
            }
        }

        for (int i = 0; i < 36; i++)
        {
            recvbuf[i] = NULL;
        }

        GetFromClient = recv(ClientSocket, recvbuf, recvbuflen, 0);
        for (int i = 0; i < sizeof(recvbuf); i++)  //zamiana duzych liter na male (kompatybilnosc)
        {
            recvbuf[i] = std::tolower(recvbuf[i]);
        }

        bool traf = 0;
        if (GetFromClient > 1 and CanPlay == true) {    //sprawdzamy ile jest znakow (jesli gracz nie jest obserwatorem)
            //wiecej niz jeden znak - sprawdzamy czy klient zgadl haslo        UWAGA NIE ROB wyniki[id][cos tam] tylko if wyniki[xxx][0] == id to wtedy wyniki[xxx][cos tam]
            if (recvbuf == slowo)
            {
                std::cout << "Zgadles slowo!" << std::endl;
                visualization_slowo = slowo;
                std::cout << visualization_slowo << std::endl;

                for (int i = 0; i < liczba_klientow; i++)
                {
                    if (wyniki[i][0] == id)
                    {
                        wyniki[i][1] = wyniki[i][1] + 5;
                        Powiadom(id, 2);
                        break;
                    }
                }
                GenNewWord();

                int dl = visualization_slowo.length();    //wyslij stan odgadnietego slowa do serwera
                char sendbuf[35];
                for (int i = 0; i < 35; i++)
                {
                    sendbuf[i] = NULL;
                    if (i < visualization_slowo.length())
                    {
                        sendbuf[i] = visualization_slowo[i];   //pakujemy slowo w wersji wizualizacja
                    }
                }

                for (int i = 0; i < liczba_klientow; i++)   //wysylamy do kazdego
                {
                    int SendToClient = send(KLIENCI[i], sendbuf, visualization_slowo.length(), 0);
                    if (SendToClient == SOCKET_ERROR) {
                        Powiadom(id, 3);
                        DisqualificationWyniki(id);
                        BreakConnection(ClientSocket);
                        return;
                    }
                }
            }
            else
            {
                std::cout << "To nie to slowo!" << std::endl;
                for (int i = 0; i < liczba_klientow; i++)
                {
                    if (wyniki[i][0] == id)
                    {
                        wyniki[i][1] = wyniki[i][1] - 1;
                        wyniki[i][2] = wyniki[i][2] - 1;
                        Powiadom(id, 1);
                        break;
                    }
                }
            }
        }
        else if (GetFromClient == 1 and CanPlay == true and recvbuf[0] != '-')
        {
            //jeden znak - sprawdzamy czy klient zgadl litere
            for (int i = 0; i < slowo.length(); i++)
            {
                if (slowo[i] == recvbuf[0])
                {
                    traf = 1;
                    if (visualization_slowo[i] == recvbuf[0])
                    {
                        std::cout << "Ta litera juz byla" << std::endl;
                        break;
                    }
                    else
                    {
                        for (int i =0; i < slowo.length(); i++)
                        {
                            if (slowo[i] == recvbuf[0])
                            {
                                visualization_slowo[i] = recvbuf[0];   //uzupe³niamy
                            }
                        }
                        for (int i = 0; i < liczba_klientow; i++)
                        {
                            if (wyniki[i][0] == id)
                            {
                                wyniki[i][1] = wyniki[i][1] + 1;
                                Powiadom(id, 0);
                                break;
                            }
                        }
                        break;
                    }
                }
            }
            if (traf == 0)
            {
                //std::cout << "test" << std::endl;
                for (int i = 0; i < liczba_klientow; i++)
                {
                    if (wyniki[i][0] == id)
                    {
                        wyniki[i][1] = wyniki[i][1] - 1;
                        wyniki[i][2] = wyniki[i][2] - 1;
                        Powiadom(id, 1);
                        break;
                    }
                }
                //std::cout << "koniec test" << std::endl;
            }
            //std::cout << visualization_slowo << std::endl;
            //std::cout << slowo << std::endl;
        }
        else 
        {
            //ujemna wartoœæ - klient jest roz³¹czany
            DisqualificationWyniki(id);
            Powiadom(id, 3);
            BreakConnection(ClientSocket);
            return;
        }
        
        for (int i = 0; i < IleGraczy; i++)   //wypisanie jako kontrola
        {
            for (int x = 0; x < 3; x++)
            {
                std::cout << wyniki[i][x] << " ";
            }
            std::cout << std::endl;
        }

        int s = IleGraczy;
        for (int i = 0; i < IleGraczy; i++)   //sprawdŸ, czy wszyscy (poza jednym) przegrali
        {
            if (wyniki[i][2] == 0)
            {
                s--;
            }
        }
        std::cout << s << std::endl;
        if (s <= 1)
        {
            std::cout << "Koniec gry" << std::endl;

            // shutdown the connection since we're done

            int iResult = shutdown(ClientSocket, SD_SEND);
            if (iResult == SOCKET_ERROR) {
                printf("shutdown failed with error: %d\n", WSAGetLastError());
                closesocket(ClientSocket);
                WSACleanup();
            }
        }
    }
    while (true);
}

int __cdecl main(void)
{
    srand(time(0)); // start generate random values (we will need it to GenNewWord function)
    GenNewWord();

    setlocale(LC_CTYPE, "Polish"); // polskie znaki

    //inicjalizacja serwera
    WSADATA wsaData;
    int iResult;

    SOCKET ListenSocket = INVALID_SOCKET;
    SOCKET ClientSocket = INVALID_SOCKET;

    struct addrinfo* result = NULL;
    struct addrinfo hints;

    int iSendResult;

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return 1;
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    // Resolve the server address and port
    iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
    if (iResult != 0) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
        return 1;
    }

    // Create a SOCKET for the server to listen for client connections.
    ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (ListenSocket == INVALID_SOCKET) {
        printf("socket failed with error: %ld\n", WSAGetLastError());
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }

    // Setup the TCP listening socket
    iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        printf("bind failed with error: %d\n", WSAGetLastError());
        freeaddrinfo(result);
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    freeaddrinfo(result);

    iResult = listen(ListenSocket, 10);
    if (iResult == SOCKET_ERROR) {
        printf("listen failed with error: %d\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    //dodawanie graczy do gry
    do
    {
        // Accept a client socket
        ClientSocket = accept(ListenSocket, NULL, NULL);
        if (ClientSocket == INVALID_SOCKET) {
            printf("accept failed with error: %d\n", WSAGetLastError());
            closesocket(ListenSocket);
            WSACleanup();
            return 1;
        }

        KLIENCI[liczba_klientow] = ClientSocket;

        BiggerWyniki();

        bool isUsed = false;    //sprawdzamy, czy to juz bylo
        //wyniki[liczba_klientow][0] = liczba_klientow;   //"id"
        for (int i = 0; i <= liczba_klientow; i++)      //pierwsza petla przelatuje przez wszystkie mozliwe id (0, 1, 2 ... liczba_klientow), a druga sprawdza, czy daje id juz bylo
        {                                               //jesli tego id nie bylo to zostaje ono nadane temu nowemu klientowi
            isUsed = false;
            for (int a = 0; a < liczba_klientow; a++)
            {
                if (wyniki[a][0] == i)
                {
                    isUsed = true;
                }
            }
            if (isUsed == false)
            {
                wyniki[liczba_klientow][0] = i;
                break;
            }
        }
        wyniki[liczba_klientow][1] = 0;         //punkty
        wyniki[liczba_klientow][2] = 10;     //hp

        std::thread(gra, ClientSocket, wyniki[liczba_klientow][0]).detach();  //osobny watek dla kazdego gracza

        liczba_klientow++;

        std::cout << "Kolejny gracz do³¹czy³ do gry!" << std::endl;

    } while (true);

    // shutdown the connection since we're done
    iResult = shutdown(ClientSocket, SD_SEND);
    if (iResult == SOCKET_ERROR) {
        printf("shutdown failed with error: %d\n", WSAGetLastError());
        closesocket(ClientSocket);
        WSACleanup();
        return 1;
    }

    // No longer need server socket
    closesocket(ListenSocket);

    // cleanup
    closesocket(ClientSocket);
    WSACleanup();

    return 0;
}