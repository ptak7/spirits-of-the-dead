#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <cstring>
#include <arpa/inet.h>

#include "Stage.h"
#include "Client.h"
#include "TcpServer.h"

#define BUF_SIZE 65536

using namespace std;

//póki co struktura - wszystko publiczne
struct GameManager 
{
    const int playersToStart = 1;

    TcpServer server;
    bool gameCanBeStarted;
    bool gameStarted;
    vector<Client> clients;
    vector<Client> clientsLobby;
    int readyPlayersCount;
    int stageNumber;
    vector<Stage> stages;
    int stagesBeforeTimer;

    GameManager(TcpServer &serv)
    {
        server = serv;
        gameCanBeStarted = false;
        gameStarted = false;
        readyPlayersCount = 0;
        stageNumber = 0;
        stagesBeforeTimer = 0;
        loadStages();
        createPipe();
    }

    void loadStages()
    {
        vector<string> ans = {"Yes", "No" };
        Stage s = Stage("Example text\nExample text 2\n", ans);
        stages.push_back(s);

        vector<string> ans2 = {"Pro", "Noob" };
        Stage s2 = Stage("Example question\nExample question 2\n", ans2);
        stages.push_back(s2);    

        vector<string> ans3 = {"Win", "Lose" };
        Stage s3 = Stage("Example question\nExample question 2\n", ans3);
        stages.push_back(s3);        
    }

    void createPipe()
    {
        int fd[2];

        int result = pipe(fd);
        if (result == -1)
            error("pipe() error!");

        result = dup2(fd[0], PIPE_READ);
        if (result == -1)
            error("dup2() error!");

        result = dup2(fd[1], PIPE_WRITE);
        if (result == -1)
            error("dup2() error!");

        server.addEpollEvent(PIPE_READ);
    }

    string getClientsInfo()
    {
        stringstream ss;
        ss << "Players online: " << clients.size() << endl
           << "Players ready: " << readyPlayersCount << endl;

        return ss.str(); 
    }

    string getClientsInLobbyInfo()
    {
        stringstream ss;
        ss << "Players waiting: " << clientsLobby.size() << endl;

        return ss.str();
    }

    string getStageAnswersInfo()
    {
        stringstream ss;
        ss << "Answers: " << getPlayersWhoAnsweredNumber() << "/" << clients.size() << endl;
        ss << "Most popular answer: " << getStage().getMostPopularAnswer() << endl;
        
        return ss.str();
    }

    int getPlayersWhoAnsweredNumber()
    {
        int count = 0;
        for (int i = 0; i < (int)clients.size(); i++)
        {
            if (clients[i].answered)
                count++;
        }

        return count;
    }

    void gameStartCheck()
    {
        if ((int)clients.size() >= playersToStart && (int)clients.size() == readyPlayersCount)
        {
            cout << "GameManager: Enough players to start the game! Let's play!" << endl;
            gameCanBeStarted = true;
        }
    }

    bool canLoadNextStage()
    {
         if (!getStage().shown) 
            return true;

        return false;
    }

    Stage& getStage()
    {
        return stages[stageNumber];
    }

    void launchStage()
    {
        getStage().show();
        sendMessageToAll(getStage().getStageText());
        letClientsAnswer();
    }

    void letClientsAnswer()
    {   
        for (int i = 0; i < (int)clients.size(); i++)
            clients[i].answered = false;
    } 

    void setClientName(Client &client, string name)
    {
        client.name = name;
        cout << "Set name of " << client.address << ":" << client.port << " fd: " << client.fd << " to '" << name << "'" << endl; 
    }

    void setClientReady(Client &client)
    {
        if (client.readyToPlay)
            return;

        client.readyToPlay = true;
        readyPlayersCount++;

        sendMessageToAll(getClientsInfo());
        cout << "Player: " << client.name << " is ready!" << endl; 

        gameStartCheck();
    }

    void setClientNotReady(Client &client)
    {
        if (!client.readyToPlay)
            return;

        client.readyToPlay = false;
        readyPlayersCount--;

        sendMessageToAll(getClientsInfo());
        cout << "Player: " << client.name << " is busy now!" << endl;     
    }

    void startGame()
    {
        gameStarted = true;
        gameCanBeStarted = false;
        sendMessageToAll("The game has started!\nGood luck\n");
    }

    void checkIfAllAnswered()
    {
        if (getStage().getAnswersCount() == (int)clients.size())
        {
            stageNumber++;
            stagesBeforeTimer++;
        }

        if (stageNumber == (int)stages.size())
            endGame();
    }

    void endGame()
    {
        cout << "The game has finished! Reseting server..." << endl;
        gameStarted = false;
        stageNumber = 0;
        readyPlayersCount = 0;
        
        letClientsAnswer();

        for (Client &c : clients)
            c.readyToPlay = false;
        
        for (Stage &s : stages)
            s.reset();

        for (Client &c : clientsLobby)
            clients.push_back(c);

        clientsLobby.clear();

        sendMessageToAll(getClientsInfo());
    }

    void showClients()
    {
        stringstream ss;
        ss << "Clients in game info:" << endl;
        for (Client c : clients)
            ss << c.toString() << endl;
        
        ss << "Clients in lobby info:" << endl;
        for (Client c : clientsLobby)
            ss << c.toString() << endl;

        cout << ss.str();
    }

    void showStages()
    {
        stringstream ss;
        ss << "Stages info:" << endl;
        for (Stage s : stages)
            ss << s.toString() << endl;

        cout << ss.str();
    }

    void showServer()
    {
        stringstream ss;
        ss << "Server info:" << endl;
        ss << server.toString() << endl;

        cout << ss.str();
    }

    void showGameManager()
    {
        stringstream ss;
        ss << "Game manager:" << endl;
        ss << "Players to start: " << playersToStart << endl;
        ss << "Players online: " << clients.size() << endl;
        ss << "Ready players: " << readyPlayersCount << endl;        
        ss << "Can start game: " << gameCanBeStarted << endl;
        ss << "Game started: " << gameStarted << endl;
        ss << "Actual stage: " << stageNumber << endl;
        ss << "Stages before timer: " << stagesBeforeTimer << endl;

        cout << ss.str();
    }


//==========================================================================
    
    void update()
    {
        if (gameCanBeStarted)
            startGame();
        if (gameStarted && canLoadNextStage())
            launchStage();

        epollWait();

        if (server.event.events == EPOLLIN && server.event.data.fd == server.serverSocket)
            handleNewClients();
        else if (server.event.events == EPOLLIN && server.event.data.fd == 0)
            handleServerRequests();
        else if (server.event.events == EPOLLIN && server.event.data.fd == PIPE_READ)
            handleTimers();
        else
        {
            handleClients();
            handleClientsLobby();
        }
    }

    void handleNewClients()
    {
        addNewClient();

        if (!gameStarted)
            sendMessageToAll(getClientsInfo());      
        else 
            sendMessageToAllLobby(getClientsInLobbyInfo());
    }

    void epollWait()
    {
        int result = epoll_wait(server.epollFd, &server.event, 1, -1);
        if (result == -1)
            error("epoll_wait() error!");
    }

    //póki co można się łączyć z tego samego IP kilka razy
    //zrobić możliwość reconnectowania z tego samego ip
    void addNewClient()
    {
        Client client = acceptClient();
        // if (isAlreadyConnected(client))
        // {
        //     removeCheater(client);
        //     return;
        // }

        server.addEpollEvent(client.fd);
        
        if (!gameStarted)
        {
            clients.push_back(client);
            writeMessage("Hello! It's 'Spirit of the Dead' text game. Enjoy!\n", client);
        }
        else
        {
            clientsLobby.push_back(client);
            writeMessage("Hello! It's 'Spirit of the Dead' text game. Enjoy!\nThe game has already started, so you have to wait until the next round.\n", client);            
        }

        cout << "New connection from: " << client.address << ":" << client.port << " fd: " <<  client.fd << endl;
    }

    bool isAlreadyConnected(Client client)
    {
        for (Client c : clients)
        {
            if (c.address == client.address)
                return true;
        }

        for (Client c : clientsLobby)
        {
            if (c.address == client.address)
                return true;
        }

        return false;
    }

    void removeCheater(Client client)
    {
        cout << "Client: " << client.address << ":" << client.port << " fd: " << client.fd << " is trying to cheat! Removing him..." << endl;
        writeMessage("Bye bye, cheater!\n", client);
        close(client.fd);
    }

    Client acceptClient()
    {
        int socket;
        sockaddr_in clientInfo;
        socklen_t clientInfoSize = sizeof(clientInfo);

        socket = accept(server.serverSocket, (sockaddr*)&clientInfo, &clientInfoSize);
        if (socket == -1)
            error("accept() error!");

        return Client(socket, inet_ntoa(clientInfo.sin_addr), ntohs(clientInfo.sin_port));
    }

    void handleServerRequests()
    {
        char buf[BUF_SIZE];

        if(!readMessage(0, buf))
            error("read() error!");

        processMessageStdIn(buf);        
    }

    void processMessageStdIn(string message)
    {
        if (message.find("server") != string::npos)    
            showServer();
        else if (message.find("clients") != string::npos)    
            showClients();
        else if (message.find("stages") != string::npos)    
            showStages();
        else if (message.find("manager") != string::npos)    
            showGameManager();
    }

    void handleTimers()
    {
        char buf[BUF_SIZE];
        
        readPipe(PIPE_READ, buf);
        cout << buf << endl;

        if (stagesBeforeTimer > 0)
        {
            stagesBeforeTimer--;
            return;
        }

        getStage().timesUp(clients.size());
        stageNumber++;

        if (stageNumber == (int)stages.size())
            endGame();
    }

    void removeClient(int n)
    {
        cout << "Removing client: " << clients[n].address << ":" << clients[n].port << " fd: " << clients[n].fd << endl;
        close(clients[n].fd);
        clients.erase(clients.begin() + n);

        if ((int)clients.size() == 0)
            endGame();
    }

    void removeClientFromLobby(int n)
    {
        cout << "Removing client: " << clientsLobby[n].address << ":" << clientsLobby[n].port << " fd: " << clientsLobby[n].fd << endl;
        close(clientsLobby[n].fd);
        clientsLobby.erase(clientsLobby.begin() + n);    
    }

    bool readMessage(int fd, char* buf)
    {
        memset(buf, 0, BUF_SIZE);

        int msgSize = read(fd, buf, BUF_SIZE);
        if (msgSize < 1)
            return false;

        return true;
    }

    void readPipe(int fd, char *buf)
    {
        memset(buf, 0, BUF_SIZE);
        read(fd, buf, BUF_SIZE);
    }

    void error(string errorMessage) 
    {
        perror(errorMessage.c_str());
        exit(0);
    }

    bool writeMessage(string message, Client &client)
    {
        int result = write(client.fd, message.c_str(), message.size());
        if (result == -1)
            return false;

        return true;
    }

    void sendMessageToAll(string message)
    {
        for (int i = 0; i < (int)clients.size(); i++)
        {
            if (!writeMessage(message, clients[i]))
                removeClient(i);
        }
    }

    void sendMessageToAllLobby(string message)
    {
        for (int i = 0; i < (int)clientsLobby.size(); i++)
        {
            if (!writeMessage(message, clientsLobby[i]))
                removeClientFromLobby(i);
        }
    }

    void sendMessageToAll(string message, Client &sender)
    {
        for (int i = 0; i < (int)clients.size(); i++)    
        {
            if (clients[i] == sender)
                continue;

            if (!writeMessage(message, clients[i]))
                removeClient(i);
        }
    }

    void sendMessageToAllLobby(string message, Client &sender)
    {
        for (int i = 0; i < (int)clientsLobby.size(); i++)
        {
            if (clientsLobby[i] == sender)
                continue;

            if (!writeMessage(message, clientsLobby[i]))
                removeClientFromLobby(i);
        }
    }

    void handleClients()
    {
        char buf[BUF_SIZE];
        
        for (int i = 0; i < (int)clients.size(); i++)
        {
            if (server.event.events == EPOLLIN && server.event.data.fd == clients[i].fd)
            {
                if(!readMessage(clients[i].fd, buf))
                {
                    removeClient(i);
                    break;
                }

                if (!gameStarted)       
                    processMessageBeforeStart(buf, clients[i]);
                else
                    processMessageInGame(buf, clients[i]);
            }
        }
    }

    void handleClientsLobby()
    {
        char buf[BUF_SIZE];

        for (int i = 0; i < (int)clientsLobby.size(); i++)
        {
            if (server.event.events == EPOLLIN && server.event.data.fd == clientsLobby[i].fd)
            {
                if (!readMessage(clientsLobby[i].fd, buf))
                {
                    removeClientFromLobby(i);
                    break;
                }

                processMessageInLobby(buf, clientsLobby[i]);
            }
        }
    }

    void processMessageBeforeStart(string message, Client &client)
    {
        if (message.find("name") != string::npos)
            setClientName(client, message.substr(message.find("name")+5));
    
        else if (message.find("notready") != string::npos)
            setClientNotReady(client);
        
        else if (message.find("ready") != string::npos)
            setClientReady(client);

        else if (message.find("chat") != string::npos)
            sendMessageToAll(client.name + ": " + message.substr(message.find("chat")+5), client);
    }

    void processMessageInGame(string message, Client &client)
    {
        if (client.answered)
            return;        

        for (int i = 0; i < (int)getStage().answers.size(); i++)
        {
            if (message.find(getStage().answers[i]) != string::npos)
            {
                cout << client.name << " choice is: " << getStage().answers[i] << endl;
                client.answered = true;
                getStage().answersStats[i]++;
                sendMessageToAll(getStageAnswersInfo());
                checkIfAllAnswered();
                return;
            }
        }

        writeMessage("Your answer is not correct!\n", client);
    }

    void processMessageInLobby(string message, Client &client)
    {
        if (message.find("chat") != string::npos)
            sendMessageToAllLobby(client.name + ": " + message.substr(message.find("chat")+5), client);
    }
};