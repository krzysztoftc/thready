#include <iostream>
#include <ncurses.h>
#include <pthread.h>
#include <list>
#include <ctime>
#include <unistd.h>
#include <cstring>

using namespace std;

class Client;

/***************
 * GLOBAL VARIABLES
***************/
int rows, cols;
bool exit_var = false;
int pump_queue[3] = {0};
int fuel_state = 50;
bool cistern = false;
list<Client*> clients;
list<Client*> cash_list;
int cistern_pos = 2000;

char cistern_fuel[] = "##########";

/***************
 * MUTEX
 **************/

pthread_mutex_t mutex_pump[3] = {PTHREAD_MUTEX_INITIALIZER,PTHREAD_MUTEX_INITIALIZER,PTHREAD_MUTEX_INITIALIZER };
pthread_mutex_t mutex_fuel = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_queses = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_cash = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_cash_queue = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_refresh = PTHREAD_MUTEX_INITIALIZER;

/*************
 * CONDITIONAL
 */
pthread_cond_t empty = PTHREAD_COND_INITIALIZER;



/***************
 * CLASSES
 *************/

enum place{
    pump1,
    pump2,
    pump3,
    cash,
    none
};

class Client{

public:

    static int counter;
    int id;
    char color;
    place position;
    pthread_t th_tank;
    bool cash = false;
    int fuel;

    Client(){
        Client::counter++;
        id = Client::counter %100;
        color = 1 + rand() % 6;
        fuel = 30 + rand() % 30;

        pthread_mutex_lock(&mutex_queses);
            if (pump_queue[pump1] <= pump_queue[pump2] && pump_queue[pump1] <= pump_queue[pump3]){
                position = pump1;
                pump_queue[pump1]++;
            }

            else if (pump_queue[pump2] <= pump_queue[pump3] && pump_queue[pump2] <= pump_queue[pump1]){
                position = pump2;
                pump_queue[pump2]++;
            }

            else {
                position = pump3;
                pump_queue[pump3]++;
            }
        pthread_mutex_unlock(&mutex_queses);

        pthread_create(&th_tank, nullptr, thread_tank, this);
        pthread_detach(th_tank);
    }

    static void* thread_tank(void *obj){
        Client *client = (Client*) (obj);
        client -> tank();
    }


    void tank() {
        int time_cash = 3000000;
        int time_tank = 50000;
        pthread_mutex_lock(&mutex_pump[position]);

        for (int i = 0; i < fuel; i++) {

            pthread_mutex_lock(&mutex_fuel);
            while (fuel_state <= 0 || cistern) {
                pthread_cond_wait(&empty, &mutex_fuel);
            }
            fuel_state--;
            pthread_mutex_unlock(&mutex_fuel);
            usleep(time_tank);
        }

        int pump_number = position;

        cash = true;
        pthread_mutex_lock(&mutex_cash_queue);
        cash_list.push_back(this);
        pthread_mutex_unlock(&mutex_cash_queue);

        pthread_mutex_lock(&mutex_cash);
        usleep(time_cash);
        pump_queue[position]--;


        pthread_mutex_lock(&mutex_cash_queue);
        cash_list.pop_front();
        pthread_mutex_unlock(&mutex_cash_queue);

        position = none;
        pthread_mutex_unlock(&mutex_cash);
        pthread_mutex_unlock(&mutex_pump[pump_number]);

    }
};

/**************
 * STATIC VARIABLES */

int Client::counter = 0;

/************
 * Functions
 */

void refresh_view(){


    clear();

    getmaxyx(stdscr,rows,cols);
    start_color();

    init_pair(1,COLOR_WHITE, COLOR_RED);
    init_pair(2,COLOR_WHITE, COLOR_GREEN);
    init_pair(3,COLOR_WHITE, COLOR_YELLOW);
    init_pair(4,COLOR_WHITE, COLOR_BLUE);
    init_pair(5,COLOR_WHITE, COLOR_MAGENTA);
    init_pair(6,COLOR_WHITE, COLOR_CYAN);
    init_pair(7,COLOR_WHITE, COLOR_BLACK);
    init_pair(8,COLOR_BLACK, COLOR_WHITE);
    init_pair(9,COLOR_BLACK, COLOR_RED);
    init_pair(10,COLOR_BLACK, COLOR_YELLOW);
    init_pair(11,COLOR_BLACK, COLOR_GREEN);

    attron(COLOR_PAIR(7));

    mvprintw(10, 15, "Dystrybutor 1: ");
    mvprintw(12, 15, "Dystrybutor 2: ");
    mvprintw(14, 15, "Dystrybutor 3: ");

    mvprintw(7, 0, "Kasa: ");
    mvprintw(2, 0, "Paliwa w zbiorniku: %d", fuel_state);

    if (fuel_state < 200) {
        attron(COLOR_PAIR(9));
    }

    else if (fuel_state < 600) {
        attron(COLOR_PAIR(10));
    }

    else {
        attron(COLOR_PAIR(11));
    }

    if (fuel_state)
    for (int i = 0 ; i <= fuel_state / 50; i++){
        mvprintw(3, i, " ");
    }

    attroff(COLOR_PAIR(11));
    mvprintw(5,0, "Miejsce dla cysterny: ");

    if(cistern_pos < cols){
        attron(COLOR_PAIR(9));
        pthread_mutex_lock(&mutex_refresh);
            mvprintw(6,cistern_pos, cistern_fuel);
        pthread_mutex_unlock(&mutex_refresh);
   }


    while (!clients.empty() && clients.front() -> position == none){
        delete clients.front();
        clients.pop_front();
    }

    int pump_queue[3]={0};
    for(Client * client : clients){
        if (client -> position == cash){
        }

        else if (client -> position != none  ){
            attron(COLOR_PAIR(client->color));
            mvprintw(11 + (client->position)*2, 15 + 4*pump_queue[client->position]++, "%02d", client -> id);
        }
    }

    int cash_queue = 0;
    for(Client * client : cash_list){
        attron(COLOR_PAIR(client->color));
        mvprintw(8, 7 + 2*cash_queue++, "|");
    }

    attron(COLOR_PAIR(7));
    refresh();
}


/***************
 * Threads
 ***************/


void *thread_cistern(void *args){
    char c[] = "##########";
    char s[] = "          ";
    cistern_pos = cols - 10;
    memcpy(cistern_fuel, &c, 10);

    while (cistern_pos > 10){
        pthread_mutex_lock(&mutex_refresh);
            cistern_pos -- ;
        pthread_mutex_unlock(&mutex_refresh);
            usleep(50000);
    }

    cistern = true;

    for (int i = 10; i > 0; i--) {
        memcpy(cistern_fuel, &c, i);
        memcpy(cistern_fuel + i, &s, 10 - i);
        pthread_mutex_lock(&mutex_fuel);

        if (fuel_state >= 950) {
            fuel_state = 1000;
            pthread_mutex_unlock(&mutex_fuel);
            break;
        }

        fuel_state += 50;

        pthread_mutex_unlock(&mutex_fuel);

        usleep(500000);
    }
    cistern = false;
    pthread_cond_broadcast(&empty);

    while (cistern_pos > 0) {
        pthread_mutex_lock(&mutex_refresh);
        cistern_pos--;
        pthread_mutex_unlock(&mutex_refresh);
        usleep(50000);
    }


    cistern_pos = 2000;

    return nullptr;
}

void *thread_keyboard(void *args){

    while(!exit_var){
        char c;
        c = getch();

        exit_var = c == 'q';

        if (c == 'a') {
            clients.push_back(new Client());
        }

        pthread_t th_cistern;

        if (c == 'c' && cistern_pos >= 2000){
            pthread_create(&th_cistern, nullptr, &thread_cistern, nullptr);
            pthread_detach(th_cistern);
        }


        c = 0;
    }
    return nullptr;
}

void *thread_refresh(void *args){

    while(!exit_var){
        refresh_view();
        usleep(100000);
    }
    return nullptr;
}


int main() {
    srand(time(nullptr));
    initscr();
    noecho();
    curs_set(0);

    clients.push_back(new Client);
    pthread_t th_keyboard, th_refresh;
    pthread_create(&th_keyboard, nullptr, thread_keyboard, nullptr);
    pthread_create(&th_refresh, nullptr, thread_refresh, nullptr);

    pthread_join(th_refresh, nullptr);
    pthread_join(th_keyboard, nullptr);

    for (Client *client : clients){
        delete(client);
    }

    pthread_cond_destroy(&empty);
    return 0;
}

