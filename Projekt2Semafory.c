#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <semaphore.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

bool debug=false;
int klienci, miejsca, krzesla, co_ile_nowy_klient=-1,fryzjer_strzyzenie=-1;
bool koniec=false;
bool fotel_zajety=false;
bool spiacy_fryzjer;
int id_obslugiwanego_klienta=-1;
int ilu_nie_weszlo;
struct lista
{
    int id_klienta;
    struct lista *next;

};
struct lista *zrezygnowali=NULL;
struct lista *w_poczekalnia=NULL;

sem_t siedziane; // klienci w poczekalni
sem_t ciecie; // czy fryzjer aktualnie kogos strzyze


sem_t fotel; //fotel
pthread_mutex_t pierwsze_miejsce;
pthread_mutex_t poczekalnia; //mutex do kontrolowania miejsc dosepnych w poczekalni
pthread_mutex_t fryzjer_co_robi; //kontroluje czy fryzjer jest w stanie snu lub pracy
pthread_mutex_t klient_ostrzyzony; //klient po zakoñczeniu scinania

void spanko(int kto, int ile)
{
    time_t t;
    srand( time(&t));
    if(kto==0)
    {
        
        //printf("bruh %d\n",r);
        if(ile==-1)
            usleep((rand() % 5 + 1)*100);
        else
            usleep(ile*100);

    }
    else
    {
        if(ile==-1)
            usleep((rand() % 5 + 3)*100);
        else
            usleep(ile*100);
    }
}

void poczekalnia_wypisz()//wypisanie wszystkich klientów w poczekalni
{
    struct lista *p;
    printf("Poczekalnia: ");
    p=w_poczekalnia;
    while(p!=NULL)
    {
        printf("%d ",p->id_klienta);
        p=p->next;
    }
    printf("\n");
}
void zrezygnowali_wypisz()//wypisanie klientow ktorzy zrezygnowali
{
    struct lista *p;
    p=zrezygnowali;
    printf("Zrezygnowali: ");
    while(p!=NULL)
    {
        printf("%d ",p->id_klienta);
        p=p->next;
    }
    printf("\n");
}
void dodaj_na_koniec_kolejki(struct lista **w_poczekalni,int klient)//dodanie klienta na koniec kolejki
{
    if(*w_poczekalni==NULL)
    {
        *w_poczekalni=(struct lista *) malloc(sizeof(struct lista));
        (*w_poczekalni)->id_klienta=klient;
        (*w_poczekalni)->next=NULL;
    }
    else
    {
    struct lista *p=*w_poczekalni;
        while(p->next!=NULL)
        {
            p=p->next;
        }
        p->next=(struct lista *) malloc(sizeof(struct lista));
        p->next->id_klienta=klient;
        p->next->next=NULL;
    }
    if(debug==true)
        poczekalnia_wypisz();
}
void dodaj_na_koniec_do_zrezygnowanych(struct lista **zrezygnowali, int klient)//dodanie klienta na koniec listy osob ktore zrezygnowalu
{
    if(*zrezygnowali==NULL)
    {
        *zrezygnowali=(struct lista *) malloc(sizeof(struct lista));
        (*zrezygnowali)->id_klienta=klient;
        (*zrezygnowali)->next=NULL;
    }
    else
    {
    struct lista *p=*zrezygnowali;
        while(p->next!=NULL)
        {
            p=p->next;
        }
        p->next=(struct lista *) malloc(sizeof(struct lista));
        p->next->id_klienta=klient;
        p->next->next=NULL;
    }
    zrezygnowali_wypisz();
}
void usun_pierwszego_z_kolejki()//klient jest obecnie strzyzony
{
    struct lista *p;
    p=w_poczekalnia;

    w_poczekalnia=p->next;
    free(p);
    if(debug==true)
        poczekalnia_wypisz();
}
void *Klient(void *numer_klienta)
{

    spanko(0,co_ile_nowy_klient);

    int id=*(int *)numer_klienta;
    if(pthread_mutex_lock(&poczekalnia)!=0)//klient w poczekalni wiec ja blokujemy, bo beda zmieniane dane
    {
        fprintf(stderr,"ERROR; return code from poczekalnia1 pthread_mutex_lock() is %d\n",errno);
        exit(EXIT_FAILURE);
    }
    if(miejsca>0) //jesli sa wolne miejsca, wejdz
    {
        

        miejsca--;

        printf("Res:%d WRomm: %d/%d [in: %d]-wszedl do poczekalni\n",ilu_nie_weszlo,krzesla-miejsca,krzesla,id_obslugiwanego_klienta);
        
        
        dodaj_na_koniec_kolejki(&w_poczekalnia,id);
        
        if(pthread_mutex_unlock(&poczekalnia)!=0) //zwalnia poczekalnie
        {
            fprintf(stderr,"ERROR; return code from poczekalnia1 pthread_mutex_lock() is %d\n",errno);
            exit(EXIT_FAILURE);
        }


        if(sem_post(&siedziane)!=0) //oznajmia fyzjerowi, ze ludzie czekaja
        {
            fprintf(stderr,"ERROR; return code from siedziane sem_post() is %d\n",errno);
            exit(EXIT_FAILURE);
        }

        while(w_poczekalnia->id_klienta!=id) //wymuszenie braku wyscigu
                usleep(1);
           

        if(sem_wait(&ciecie)!=0) //czeka az fryzjer pozwoli wejsc do srodka
        {
            fprintf(stderr,"ERROR; return code from ciecie sem_wait() is %d\n",errno);
            exit(EXIT_FAILURE);
        }

        
        if(pthread_mutex_lock(&poczekalnia)!=0) //wstrzymuje operacje, bo zmienia dane
        {
            fprintf(stderr,"ERROR; return code from poczekalnia2 pthread_mutex_lock() is %d\n",errno);
            exit(EXIT_FAILURE);
        }

        id_obslugiwanego_klienta=id;

        miejsca++;
        
        printf("Res:%d WRomm: %d/%d [in: %d]-klient jest scinany\n",ilu_nie_weszlo,krzesla-miejsca,krzesla,id_obslugiwanego_klienta);
        
        
        usun_pierwszego_z_kolejki(); //usuwamy klienta z kolejki
        

        if(pthread_mutex_unlock(&poczekalnia)!=0) //odblokowanie poczekalni
        {
            fprintf(stderr,"ERROR; return code from poczekalnia2 pthread_mutex_unlock() is %d\n",errno);
            exit(EXIT_FAILURE);
        }
  
        
        koniec=false;




    }
    else
    {
        ilu_nie_weszlo++;
        printf("Res:%d WRomm: %d/%d [in: %d]  -klient nie wszedl\n",ilu_nie_weszlo,krzesla-miejsca,krzesla,id_obslugiwanego_klienta);
        if(debug==true)
        {
            dodaj_na_koniec_do_zrezygnowanych(&zrezygnowali,id);
        }

        if(pthread_mutex_unlock(&poczekalnia)!=0) //odblokowanie poczekalni
        {
            fprintf(stderr,"ERROR; return code from poczekalnia pthread_mutex_unlock() is %d\n",errno);
            exit(EXIT_FAILURE);
        }
    }
}
void *Fryzjer()
{
    if(pthread_mutex_lock(&fryzjer_co_robi)!=0)//blokowanie obecnego stanu fryzjera
    {
        fprintf(stderr,"ERROR; return code from fryzjer_co_robi pthread_mutex_lock() is %d\n",errno);
        exit(EXIT_FAILURE);
    }

    while(!koniec) //dopoki sa klienci
    {
        if(!koniec)
        {
            if(sem_wait(&siedziane)!=0) //fryzjer czeka az go obudzi klient
            {
                fprintf(stderr,"ERROR; return code from siedziane sem_wait() is %d\n",errno);
                exit(EXIT_FAILURE);
            }

            if(sem_post(&ciecie)!=0) //jak jest obudzony to zaprasza klienta na krzeslo
            {
                fprintf(stderr,"ERROR; return code from ciecie sem_post() is %d\n",errno);
                exit(EXIT_FAILURE);
            }


            spanko(1,fryzjer_strzyzenie); //fryzjer strzyze

            if(pthread_mutex_unlock(&fryzjer_co_robi)!=0) //jak skonczyl, to odblokowuje stan
            {
                fprintf(stderr,"ERROR; return code from fryzjer_co_robi pthread_mutex_unlock() is %d\n",errno);
                exit(EXIT_FAILURE);
            }
        }
        else
            printf("Koniec pracy\n");
    }

}
int main(int argc, char *argv[])
{
    //zainicjowanie zmiennych warunkowych i mutexow

    if(pthread_mutex_init(&poczekalnia,NULL)!=0)
    {
        fprintf(stderr,"ERROR; return code from poczekalnia pthread_mutex_init() is %d\n",errno);
        exit(EXIT_FAILURE);
    }
    if(pthread_mutex_init(&fryzjer_co_robi,NULL)!=0)
    {
        fprintf(stderr,"ERROR; return code from fryzjer_co_robi pthread_mutex_init() is %d\n",errno);
        exit(EXIT_FAILURE);
    }
    int opt;
    while((opt=getopt(argc,argv,"k:c:t:f:d"))!=-1)
    {
        switch(opt)
        {
        case 'k': //ile bedzie klientow
            klienci=atoi(optarg);
            break;
        case 'c': // liczba krzesel w poczekalni
            miejsca=atoi(optarg);
            krzesla=atoi(optarg);
            break;
        case 't': // jak czesto pojawiaja sie klienci
            co_ile_nowy_klient=atoi(optarg);
            break;
        case 'f':  // czas strzyzenia
            fryzjer_strzyzenie=atoi(optarg);
            break;
        case 'd': //wypisanie poczekalni i zrezygnowanych klientow
            debug=true;
            break;
        }
    }
    pthread_t klient_watek[klienci];
    pthread_t fryzjer_watek;
    int t[klienci];
    int i;
    for(i=0;i<klienci;i++)
    {
        t[i]=i;
    }
    if(pthread_create(&fryzjer_watek,0,Fryzjer,0)!=0)
    {
        fprintf(stderr,"ERROR; return code from fryzjer_watek pthread_create is %d\n",errno);
        exit(EXIT_FAILURE);
    }
    printf("\nfryzjer \n");
    for(i=0;i<klienci;i++)
    {
        spanko(0,co_ile_nowy_klient);
        if(pthread_create(&klient_watek[i],0,Klient,(void *)&t[i])!=0)
        {
            fprintf(stderr,"ERROR; return code from klient_watek pthread_create[%d] is %d\n",i,errno);
            exit(EXIT_FAILURE);
        }
    }
    for(i=0;i<klienci;i++)
    {
        if(pthread_join(klient_watek[i],0)!=0)
        {
            fprintf(stderr,"ERROR; return code from klient_watek pthread_join[%d] is %d\n",i,errno);
            exit(EXIT_FAILURE);
        }
    }
    koniec=true;
    if(sem_post(&siedziane)!=0)
    {
        fprintf(stderr,"ERROR; return code from siedziane sem_post() Main is %d\n",errno);
        exit(EXIT_FAILURE);
    }

    if(pthread_join(fryzjer_watek, 0)!=0)
    {
        fprintf(stderr,"ERROR; return code from fryzjer_watek thread_join() is %d\n",errno);
        exit(EXIT_FAILURE);
    }

    //usuniecie mutexow
    if(pthread_mutex_destroy(&poczekalnia)<0)
    {
        fprintf(stderr,"ERROR; return code from poczekalnia pthread_mutex_destroy() is %d\n",errno);
        exit(EXIT_FAILURE);
    }
    if(pthread_mutex_destroy(&fryzjer_co_robi)<0)
    {
        fprintf(stderr,"ERROR; return code from poczekalnia pthread_mutex_destroy() is %d\n",errno);
        exit(EXIT_FAILURE);
    }
    free(zrezygnowali);
    free(w_poczekalnia);
    return 0;
}
