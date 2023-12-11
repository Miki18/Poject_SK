#define WIN32_LEAN_AND_MEAN

#include <math.h>
#include <GL/glew.h>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <string>
#include <locale.h>
#include <GLFW/glfw3.h>
#include <thread>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <vector>
#include <fstream>

// Need to link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27015"

bool Accept_Game = false;  //zmienna s³u¿¹ca tylko dod akceptacji gry
char sendbuf[35];  //konstantynopolitañczykowianeczka jest najd³u¿szym wyrazam i ma 32 litery, ale dla pewnoœci (i ³adnego wyl¹du) dam char [35]
bool enter_word = false; //zmienna pozwalaj¹ca wprowadzic slowo do wyslania
char Answerbuf[35];  //zmienna do odbierania sygnalu z serwera
char Guessing[35];   //zgadywana slowo
bool GameStarted = false;
int LiczbaGraczy = 0;
bool DoTextures = false;
int ShowingID = 0;
bool TheEnd = false;   //bool do ktorego zapiszemy, ze jest koniec gry

int** wyniki = nullptr;

GLuint texture;

GLuint loadTexture(const char* filename) {
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    // Ustaw parametry tekstury
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Wczytaj obraz
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "Nie mo¿na otworzyæ pliku: " << filename << std::endl;
        return 0;
    }

    // Wczytaj nag³ówek BMP
    char header[54];
    file.read(header, 54);
    int width = *(int*)&header[18];
    int height = *(int*)&header[22];

    // Wczytaj dane pikseli
    int size = 3 * width * height;
    std::vector<char> pixels(size);
    file.read(pixels.data(), size);

    // Utwórz teksturê
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_BGR, GL_UNSIGNED_BYTE, pixels.data());

    return texture;
}

void ustawienieTextury()
{
    GLuint texture;
    switch (wyniki[ShowingID][2]) //wczytaj texturê
    {
    case 10:
        texture = loadTexture("0.bmp");
        break;

    case 9:
        texture = loadTexture("1.bmp");
        break;

    case 8:
        texture = loadTexture("2.bmp");
        break;

    case 7:
        texture = loadTexture("3.bmp");
        break;

    case 6:
        texture = loadTexture("4.bmp");
        break;

    case 5:
        texture = loadTexture("5.bmp");
        break;

    case 4:
        texture = loadTexture("6.bmp");
        break;

    case 3:
        texture = loadTexture("7.bmp");
        break;

    case 2:
        texture = loadTexture("8.bmp");
        break;

    case 1:
        texture = loadTexture("9.bmp");
        break;

    case 0:
        texture = loadTexture("10.bmp");
        break;

    case -1:
        texture = loadTexture("disconnected.bmp");
    }
    std::string t;
    if (ShowingID == 0)
    {
        t = "Twoj wisielec";
    }
    else
    {
        t = "Wisielec przeciwnika";
    }
    ImGui::Begin("");
    ImGui::Text("%s", t.c_str());
    ImGui::Image((void*)(intptr_t)texture, ImVec2(200, 400));
    ImGui::End();
}

void ShowWyniki()
{
    if (wyniki[ShowingID][2] < 1 and ShowingID == 0)
    {
        ImGui::Begin("Lol u ded");
        ImGui::End();
    }
    else if (wyniki[ShowingID][2] < 1 and ShowingID != 0)
    {
        ImGui::Begin("Lol he's ded");
        ImGui::End();
    }
    std::string t;
    if (ShowingID == 0)
    {
        t = "Twoje wyniki:";
    }
    else
    {
        t = "Wyniki przeciwnika: ";
    }
    ImGui::Begin("");
    ImGui::Text("%s", t.c_str());
    ImGui::Text("ID: ");
    t = std::to_string(wyniki[ShowingID][0]);
    ImGui::Text("%s", t.c_str());
    ImGui::Text("Punkty: ");
    t = std::to_string(wyniki[ShowingID][1]);
    ImGui::Text("%s", t.c_str());
    ImGui::Text("Pozostale zycia: ");
    t = std::to_string(wyniki[ShowingID][2]);
    ImGui::Text("%s", t.c_str());
    ImGui::End();
}

void SerwerMess(SOCKET ConnectSocket)
{
    bool CzyMaszWyniki = false;
    do
    {
        int GetFromSerwer = recv(ConnectSocket, Answerbuf, 35, 0);  //odczytywanie tabeli
        if (GetFromSerwer > 0) {
            if (GameStarted == true)  //w trakcie gry
            {
                if (Answerbuf[0] >= char(48) and Answerbuf[0] <= char(57))
                {
                    if (CzyMaszWyniki == false)   //trzeba przerobiæ t¹ wiadomoœæ, któr¹ otrzymaliœmy na tabelê (dzieje siê to tylko na pocz¹tku gry)
                    {                       //algorytm najpierw bada jaka d³uga jest pierwsza cyfra, a potem zamienia j¹ na inta (liczba graczy). Nastepnie robi to tyle razy ile jest graczy, aby odczytaæ ich indeksy, przy czym pierwszy indeks przypisuje sobie
                        int liczba = 0;
                        int pos = 0;
                        int dl_liczby = 0;
                        wyniki[int(Answerbuf[0])][3];   //stworzenie wynikow
                        //std::cout << "Answerbuf1: " << Answerbuf[0] << std::endl;  //ile graczy
                        while (Answerbuf[dl_liczby] >= char(48) and Answerbuf[dl_liczby] <= char(57));  //jaka dluga jest pierwsza liczba (odczytujemy ci¹g 0 - 9 do momentu napotkania ' ')
                        {
                            dl_liczby++;
                        }

                        int pot = 0;

                        for (int i = dl_liczby - 1; i >= 0; i--)
                        {
                            liczba = liczba + (Answerbuf[i] - '0') * pow(10, pot);
                            pot++;
                        }

                        pos = dl_liczby + 1;

                        std::cout << liczba << std::endl;
                        LiczbaGraczy = liczba;

                        wyniki = new int* [liczba];
                        for (int i = 0; i < liczba; i++)
                        {
                            wyniki[i] = new int[3];
                        }

                        for (int i = 0; i < liczba; i++)    //odcztytujemy wszystkie pozosta³e
                        {
                            int cyfra = 0;
                            dl_liczby = 0;
                            while (Answerbuf[pos + dl_liczby] >= char(48) and Answerbuf[pos + dl_liczby] <= char(57))  //odczytujemy indeksy (pierwszy jest nasz w³asny)
                            {
                                dl_liczby++;
                            }

                            int pot = 0;

                            for (int a = pos + dl_liczby - 1; a >= pos; a--)    //SPRAWDZ
                            {
                                cyfra = cyfra + (Answerbuf[a] - '0') * pow(10, pot);
                                pot++;
                            }
                            std::cout << cyfra << std::endl;
                            pos = pos + dl_liczby + 1;
                            wyniki[i][0] = cyfra;    //id
                            wyniki[i][1] = 0;     //ilosc punktow
                            wyniki[i][2] = 10;   //hp
                        }
                        CzyMaszWyniki = true;

                        std::cout << "SPRAWDZENIE" << std::endl;

                        for (int i = 0; i < LiczbaGraczy; i++)   //wypisz (kontrola)
                        {
                            std::cout << wyniki[i][0] << " " << wyniki[i][1] << " " << wyniki[i][2] << std::endl;
                        }
                    }
                    else
                    {
                        //update wynikow, najpierw id gracza, a potem 0 - zgadl litere poprawnie; 1 - nie trafil ze zgadywaniem litery; 2 - zgadl slowo poprawnie; 3 - rozlaczony
                        int ReceiverID = 0;

                        int lenght = 0;
                        int pot = 0;

                        int command;

                        while (Answerbuf[lenght] >= char(48) and Answerbuf[lenght] <= char(57))  //odczytujemy indeksy (pierwszy jest nasz w³asny)
                        {
                            lenght++;
                        }

                        for (int i = lenght - 1; i >= 0; i--)
                        {
                            ReceiverID = ReceiverID + (Answerbuf[i] - '0') * pow(10, pot);
                            pot++;
                        }
                        lenght++;

                        for (int i = 0; i < LiczbaGraczy; i++)
                        {
                            if (wyniki[i][0] == ReceiverID)
                            {
                                std::cout << int(Answerbuf[lenght]) << std::endl;
                                switch (int(Answerbuf[lenght]))
                                {
                                case 0:
                                    std::cout << "Dodano 1" << std::endl;
                                    wyniki[i][1] = wyniki[i][1] + 1;
                                    break;
                                case 1:
                                    std::cout << "Odjeto 1" << std::endl;
                                    wyniki[i][1] = wyniki[i][1] - 1;
                                    wyniki[i][2] = wyniki[i][2] - 1;
                                    break;
                                case 2:
                                    std::cout << "Dodano 5" << std::endl;
                                    wyniki[i][1] = wyniki[i][1] + 5;
                                    break;
                                case 3:
                                    std::cout << "Disconnect" << std::endl;
                                    wyniki[i][1] = 0;
                                    wyniki[i][2] = -1;
                                    break;
                                }
                                break;
                            }
                        }

                        for (int i = 0; i < 35; i++)
                        {
                            Answerbuf[i] = NULL;
                        }

                        for (int i = 0; i < LiczbaGraczy; i++)    //wypisz (kontrola)
                        {
                            std::cout << wyniki[i][0] << " " << wyniki[i][1] << " " << wyniki[i][2] << std::endl;
                        }

                    }
                }
                else
                {
                    for (int i = 0; i < 35; i++)
                    {
                        Guessing[i] = NULL;
                        Guessing[i] = Answerbuf[i];
                        Answerbuf[i] = NULL;
                    }
                }
            }
            else    //w trakcie oczekiwania na gre
            {
                Sleep(200);
                for (int i = 0; i < 35; i++)
                {
                    Answerbuf[i] = NULL;
                }
            }
        }
    } while (true);
}

void Game(SOCKET ConnectSocket)  //czêœæ kodu odpowiadaj¹ca za komunikacjê serwer - klient
{
    int myID;
    int SendToSerwer;
    std::string str;  //zmienne

    Accept_Game = false;  //trzeba, bo inaczej siê psuje

    do
    {
        Sleep(10);   //trzeba delay, bo inaczej program dostaje zawa³u (pewnie przez sieæ)
        if (Accept_Game == true)  //jak gracz zaakceptuje to go dodaj
        {
            std::cout << Accept_Game << std::endl;
            sendbuf[0] = 'A';
            //wyslij wiadomosc o akceptacji gry i break;
            SendToSerwer = send(ConnectSocket, sendbuf, 1, 0);
            if (SendToSerwer == SOCKET_ERROR)
            {
                printf("send accept failed: %d\n", WSAGetLastError());
                closesocket(ConnectSocket);
                WSACleanup();
                exit(1);
            }
            break;
        }

    } while (true);

    do
    {
        std::cout << "Waiting" << std::endl;
        if (Answerbuf[0] == '2')    //czekaj w tej pêtli dopóki serwer nie wyœle sygna³u, ¿e mo¿na graæ
        {
            std::cout << "Break" << std::endl;
            GameStarted = true;
            break;
        }

    } while (true);

    Sleep(1000);    //musi to byæ, bo inaczej siê program zawiesza
    DoTextures = true;

    do
    {
        do
        {
            Sleep(10);   //trzeba delay, bo inaczej program wywala
            if (enter_word == true)  //jesli naklikniemy enter to podajemy slowo do wyslania
            {
                str = sendbuf;   //trzeba zamieniæ na string, bo string ma kilka fajnych funkcji, które siê tu przydadz¹
                std::cout << enter_word << std::endl;
                enter_word = false;
                if (str.length() > 35)
                {
                    std::cout << "Maksymalna d³ugoœæ s³owa wynosi 35! Spróbuj ponownie" << std::endl;   //ograniczenie
                }
                else
                {
                    break;   //jak nie ma zastrze¿eñ (s³owo nie jest za d³ugie) to idziemy dalej
                }
            }
        } while (true);

        if (wyniki[0][2] > 0)
        {
            // Send an initial buffer
            SendToSerwer = send(ConnectSocket, sendbuf, str.length(), 0);   //wysy³anie
            if (SendToSerwer == SOCKET_ERROR) {
                printf("send failed with error: %d\n", WSAGetLastError());
                closesocket(ConnectSocket);
                WSACleanup();
                exit(1);
            }
        }
        else
        {
            std::cout << "Lol, nie ¿yjesz XD" << std::endl;
        }

        //printf("Bytes Sent: %ld\n", SendToSerwer);    //kontrola (tylko do debugowania)
        int gracze_zywi = 0;

        for (int i = 0; i < LiczbaGraczy; i++)
        {
            if (wyniki[i][2] > 0)
            {
                gracze_zywi++;

                if (gracze_zywi > 2)
                {
                    break;
                }
            }
        }
        if (gracze_zywi < 2)
        {
            std::cout << "Koniec gry" << std::endl;
            TheEnd = true;
            break;
        }

        Sleep(1000);   //w sumie nie wiem po co to - niech nieco spowalnia prêdkoœæ - najwy¿ej potem usuñ

    } while (true);  //wieczna pêtla
}

int main()
{
    GLFWwindow* window;   //utwórz okno

    if (!glfwInit()) { //initiate GLFW
        fprintf(stderr, "Can't run GLFW.\n");
        exit(EXIT_FAILURE);
    }

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);   //malowanko

    window = glfwCreateWindow(1280, 720, "Hangman", NULL, NULL);  //create window (size and title)
    glfwMakeContextCurrent(window);

    if (glewInit() != GLEW_OK) { //initiate GLEW
        fprintf(stderr, "Nie mo¿na zainicjowaæ GLEW.\n");
        exit(EXIT_FAILURE);
    }

    // Inicjalizacja ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    setlocale(LC_CTYPE, "Polish"); // polskie znaki (i tak tego nie mo¿na wysy³aæ) lol

    WSADATA wsaData;                  //zmienne (zrób z nimi porz¹dek jeszcze)
    SOCKET ConnectSocket = INVALID_SOCKET;
    struct addrinfo* result = NULL,
        * ptr = NULL,
        hints;
    char recvbuf[DEFAULT_BUFLEN];
    int iResult;
    int recvbuflen = DEFAULT_BUFLEN;

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return 1;
    }

    ZeroMemory(&hints, sizeof(hints));    //do tcp
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    // Resolve the server address and port
    iResult = getaddrinfo("localhost", DEFAULT_PORT, &hints, &result);
    if (iResult != 0) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
        return 1;
    }

    // Attempt to connect to an address until one succeeds
    for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {

        // Create a SOCKET for connecting to server
        ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype,
            ptr->ai_protocol);
        if (ConnectSocket == INVALID_SOCKET) {
            printf("socket failed with error: %ld\n", WSAGetLastError());
            WSACleanup();
            return 1;
        }

        // Connect to server.
        iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
        if (iResult == SOCKET_ERROR) {
            closesocket(ConnectSocket);
            ConnectSocket = INVALID_SOCKET;
            continue;
        }
        break;
    }

    freeaddrinfo(result);

    if (ConnectSocket == INVALID_SOCKET) {
        printf("Unable to connect to server!\n");
        WSACleanup();
        return 1;
    }

    std::thread(Game, ConnectSocket).detach();  //w¹tek osobno dla komunikacji serwer - klient, osobno dla interfejsu
    std::thread(SerwerMess, ConnectSocket).detach();  //w¹ted poœwiêcony dla odbierania komunikatów od serwera, aby nie by³o sytuacji, gdzie jakiœ komunikat nie dotrze, bo w¹tek czeka
    //na funckji, która wysy³a, a ta czeka na gracza, by coœ wpisa³

    do   //wieczna pêtla interface
    {
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);  //malowanko

        //ustawienie ImGui
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (Accept_Game == false)   //do akceptacji
        {
            // Tworzenie przycisku ImGui
            if (ImGui::Button("Przycisk")) {
                Accept_Game = true; // Zmiana wartoœci zmiennej bool po naciœniêciu przycisku
            }
            //std::cout << Accept_Game << std::endl;
        }
        else if (TheEnd == true)
        {
            ImGui::Begin("Koniec Gry!");
            for (int i = 0; i < LiczbaGraczy; i++)
            {
                std::string koniec = std::to_string(wyniki[i][0]);
                ImGui::Text("ID: ");
                ImGui::Text(koniec.c_str());
                koniec = std::to_string(wyniki[i][1]);
                ImGui::Text("Wynik: ");
                ImGui::Text(koniec.c_str());
                ImGui::Text("");
            }
            ImGui::End();
        }
        else if (Accept_Game == true and GameStarted == true) //jesli zaakceptujesz to mozesz sobie wysylac slowa
        {
            ImGui::Begin("Tu wpisuj litery/slowa :)");
            if (ImGui::InputText("", sendbuf, 35, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue))
            {
                //std::cout << "przeszlo" << std::endl;
                enter_word = true;
            }
            ImGui::End();

            ImGui::Begin("Odgadniete slowo:");
            ImGui::Text(Guessing);
            ImGui::End();

            glDeleteTextures(1, &texture);   //usun texture, bo komputer zawa³u dostanie

            //dodaj jeszcze przycisk "wyniki". Bêdzie on ikrementowa³ id i pokazywal te wyniki, ktore id mamy wskazane. Jak bedzie nie moje id to pisz "gracz o ID X" a jak moje to "Twoje wyniki"
            if (ImGui::Button("NEXT")) {
                if (ShowingID + 1 == LiczbaGraczy)
                {
                    ShowingID = 0;
                }
                else
                {
                    ShowingID++;
                }
            }

            if (DoTextures)
            {
                ustawienieTextury();
                ShowWyniki(); //tutaj pokaz wyniki
            }
        }
        else if (Accept_Game == true and GameStarted == false)
        {
            ImGui::Begin("Czekaj na gre");
            ImGui::End();
        }

        ImGui::Render();    //renderowanie ImGUI
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);   //renderowanie glfw
        glfwPollEvents();
    } while (true);

    // shutdown the connection since no more data will be sent
    iResult = shutdown(ConnectSocket, SD_SEND);
    if (iResult == SOCKET_ERROR) {
        printf("shutdown failed with error: %d\n", WSAGetLastError());
        closesocket(ConnectSocket);
        WSACleanup();
        return 1;
    }

    // cleanup
    closesocket(ConnectSocket);
    WSACleanup();

    return 0;
}