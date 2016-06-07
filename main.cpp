#include <iostream>
#include <ncurses.h>
#include <pthread.h>
#include <list>
#include <ctime>
#include <unistd.h>
#include <cstring>
#include <vector>

#define last_position 150
#define move_time 40000

using namespace std;

class Client;

/***************
 * GLOBAL VARIABLES
***************/
int rows, cols;
bool exit_var = false;
int pump_queue[3] = {0};
int fuel_state = 1000;
bool cistern = false;
list<Client*> clients;
list<Client*> cash_list;
vector<bool> positions[3];
int cistern_pos = 2000;

bool pumping[3]={false, false, false};
bool bulb_state = false;

int on_station = 0;

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

pthread_mutex_t mutex_move[3] = {PTHREAD_MUTEX_INITIALIZER};
pthread_mutex_t mutex_adding = PTHREAD_MUTEX_INITIALIZER;

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
    tank,
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
    int pos;

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

            else{
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

        pos = last_position-6;

        int p = position;
        position = none;

        while (true){
            pthread_mutex_lock(&mutex_move[p]);
                if (positions[p][pos] == false && positions[p][pos-6] == false) {
                    pthread_mutex_unlock(&mutex_move[p]);
                    positions[p][pos] = positions[p][pos-6] = true;
                    break;
                }
            pthread_mutex_unlock(&mutex_move[p]);
            usleep(50000);
        }

        position = place(p) ;

        while( pos > 15 ) {
            pthread_mutex_lock(&mutex_move[position]);
                if (positions[position][pos-7] == false) {
                    positions[position][pos] = false;
                    pos--;
                    positions[position][pos] = true;
                }
            pthread_mutex_unlock(&mutex_move[position]);
            usleep(move_time);
        }

        pthread_mutex_lock(&mutex_pump[position]);
        pumping[position] = true;
        for (int i = 0; i < fuel; i++) {

            pthread_mutex_lock(&mutex_fuel);
            while (fuel_state <= 0 || cistern) {
                pthread_cond_wait(&empty, &mutex_fuel);
            }
            fuel_state--;
            pthread_mutex_unlock(&mutex_fuel);
            usleep(time_tank);
        }
        pumping[position] = false;
        int pump_number = position;

        cash = true;
        pthread_mutex_lock(&mutex_cash_queue);
        cash_list.push_back(this);
        pthread_mutex_unlock(&mutex_cash_queue);

        pthread_mutex_lock(&mutex_cash);
        usleep(time_cash);

        pthread_mutex_lock(&mutex_cash_queue);
        cash_list.pop_front();
        pthread_mutex_unlock(&mutex_cash_queue);

        pump_queue[position]--;

        pthread_mutex_unlock(&mutex_cash);
        pthread_mutex_unlock(&mutex_pump[pump_number]);

        while( pos >= 7 ) {
            pthread_mutex_lock(&mutex_move[position]);
            if (positions[position][pos-7] == false) {
                positions[position][pos] = false;
                pos--;
                positions[position][pos] = true;
            }
            pthread_mutex_unlock(&mutex_move[position]);
            usleep(move_time);
        }

        pthread_mutex_lock(&mutex_move[position]);
        positions[position][6] = false;
        pthread_mutex_unlock(&mutex_move[position]);

        position = none;
        on_station--;

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

//    for (int i=0 ; i< cols; i++)
//        for (int j =0 ; j < rows; j++ ) mvprintw(j,i," ");

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

    mvprintw(6, 0, "%s"," ____ \n/____\\ \n|KASA| \n|    | \n|____| \n");
    //mvprintw(2, 0, "Paliwa w zbiorniku: %d", fuel_state);


    mvprintw(4, 0, " _____________________");
    mvprintw(5, 0, "|");
    if (fuel_state < 200) {
        attron(COLOR_PAIR(9));
    }

    else if (fuel_state < 600) {
        attron(COLOR_PAIR(10));
    }

    else {
        attron(COLOR_PAIR(11));
    }
    int i = 0;
    if (fuel_state) {
        for (; i <= fuel_state / 50; i++) {
            mvprintw(5, i + 1, "_");
        }
    }
    attroff(COLOR_PAIR(11));

    for (; i <= 20; i++) {
        mvprintw(5, i + 1, "_");
    }
    mvprintw(5, i + 1, "|");
    //mvprintw(5,0, "Miejsce dla cysterny: ");

    if(cistern_pos < cols){

        pthread_mutex_lock(&mutex_refresh);
            mvprintw(0,cistern_pos,"/_|");
            mvprintw(1,cistern_pos-2,"__| |");
            mvprintw(2,cistern_pos-2,"|___");
            attron(COLOR_PAIR(9));
            mvprintw(1,cistern_pos+3, cistern_fuel);
            mvprintw(2,cistern_pos+3, cistern_fuel);
            attroff(COLOR_PAIR(9));
            mvprintw(3,cistern_pos,"O O       OOO");
        pthread_mutex_unlock(&mutex_refresh);
   }


    while (!clients.empty() && clients.front() -> position == none){
        delete clients.front();
        clients.pop_front();
    }

    bulb_state = !bulb_state;

    mvprintw(11, 15, "   _____");
    mvprintw(12, 15, "   |1| |");
    mvprintw(13, 15, "   | |");
    if(fuel_state && !cistern && pumping[0] && bulb_state) {
        attron(COLOR_PAIR(9));
        mvprintw(13, 19, " ");
        attroff(COLOR_PAIR(9));
    }
    else{
        mvprintw(27, 19, " ");
    }

    mvprintw(18, 15, "   _____");
    mvprintw(19, 15, "   |2| |");
    mvprintw(20, 15, "   | |");
    if(fuel_state && !cistern && pumping[1] && bulb_state) {
        attron(COLOR_PAIR(9));
        mvprintw(20, 19, " ");
        attroff(COLOR_PAIR(9));
    }
    else{
        mvprintw(27, 19, " ");
    }

    mvprintw(25, 15, "   _____");
    mvprintw(26, 15, "   |3| |");
    mvprintw(27, 15, "   | |");
    if(fuel_state && !cistern && pumping[2] && bulb_state) {
        attron(COLOR_PAIR(9));
        mvprintw(27, 19, " ");
        attroff(COLOR_PAIR(9));
    }
    else{
        mvprintw(27, 19, " ");
    }




    int pump_queue[3]={0};
    for(Client * client : clients){
        if (client -> position == cash){
        }

        else if (client -> position != none  ){
            mvprintw(14+ (client->position)*7, (client -> pos), "   ___");
            mvprintw(15+ (client->position)*7, (client -> pos), " _/|_|\\");
            attron(COLOR_PAIR(client->color));
            mvprintw(16+ (client->position)*7, (client -> pos), "|__%02d_|", client -> id);
            attroff(COLOR_PAIR(client->color));
            mvprintw(17+ (client->position)*7, (client -> pos), "O     O");
        }
    }

    int cash_queue = 0;
    for(Client * client : cash_list){
        attron(COLOR_PAIR(client->color));
        mvprintw(10, 7 + 2*cash_queue++, "|");
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
    cistern_pos = cols - 12;
    memcpy(cistern_fuel, &c, 10);

    while (cistern_pos > 10){
        pthread_mutex_lock(&mutex_refresh);
            cistern_pos -- ;
        pthread_mutex_unlock(&mutex_refresh);
            usleep(move_time);
    }

    cistern = true;

    for (int i = 10; i > 0; i--) {
        usleep(500000);
        memcpy(cistern_fuel, &c, i);
        memcpy(cistern_fuel + i -1, &s, 11 - i);
        pthread_mutex_lock(&mutex_fuel);

        if (fuel_state >= 950) {
            fuel_state = 1000;
            pthread_mutex_unlock(&mutex_fuel);
            break;
        }

        fuel_state += 50;

        pthread_mutex_unlock(&mutex_fuel);


    }
    cistern = false;
    pthread_cond_broadcast(&empty);

    while (cistern_pos > 0) {
        pthread_mutex_lock(&mutex_refresh);
        cistern_pos--;
        pthread_mutex_unlock(&mutex_refresh);
        usleep(move_time);
    }


    cistern_pos = 2000;

    return nullptr;
}

void *thread_keyboard(void *args){

    while(!exit_var){
        char c;
        c = getch();

        exit_var = c == 'q';

        if (c == 'a' && on_station < 66) {
            on_station ++;
            clients.push_back(new Client());
        }

        pthread_t th_cistern;

        pthread_mutex_lock(&mutex_adding);
        if (c == 'c' && cistern_pos >= 2000){
            pthread_create(&th_cistern, nullptr, &thread_cistern, nullptr);
            pthread_detach(th_cistern);
        }
        pthread_mutex_unlock(&mutex_adding);

        c = 0;

    }
    return nullptr;
}

void *thread_refresh(void *args){

    while(!exit_var){
        refresh_view();
        usleep(80000);
    }
    return nullptr;
}


int main() {
    srand(time(nullptr));

    for (int i = 0; i < 3; i++){
        for (int j = 0; j <= last_position; j++){
            positions[i].push_back(false);
        }
    }
    initscr();
    noecho();
    curs_set(0);

    clients.push_back(new Client);
    pthread_t th_keyboard, th_refresh;
    pthread_create(&th_keyboard, nullptr, thread_keyboard, nullptr);
    pthread_create(&th_refresh, nullptr, thread_refresh, nullptr);

    pthread_join(th_refresh, nullptr);
    pthread_join(th_keyboard,nullptr);

    while( ! clients.empty()){
        delete(clients.front());
        clients.pop_front();
    }

    echo();
    curs_set(1);
    clear();

    pthread_cond_destroy(&empty);
    endwin();
    system("reset");
    return 0;
}

