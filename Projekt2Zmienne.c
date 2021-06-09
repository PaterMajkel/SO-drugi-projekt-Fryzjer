#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <semaphore.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include <unistd.h>
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
struct lista *w_poczekalni=NULL;
pthread_cond_t zapraszam_na_fotel; //zaproszenie nowego klienta po skoñczeniu strzyzenia
pthread_cond_t obudzenie; //budzenie fryzjera
pthread_mutex_t lista; //kontrolowanie listy
pthread_mutex_t poczekalnia; //mutex do kontrolowania miejsc dosepnych w poczekalni
int bileciki_do_kontroli=0;
int obecny_bilet=0;
void losowy_czas_strzyzenia()
{
    int czas=rand()%5+1;
    usleep(czas);
}
void losowy_czas_przychodzenia()
{
    int czas=rand()%5+1;
    usleep(czas);
}
void poczekalnia_wypisz()//wypisanie wszystkich klientów w poczekalni
{
    struct lista *p;
    printf("Poczekalnia: ");
    p=w_poczekalni;
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
    if(pthread_mutex_lock(&lista)!=0)
    {
	fprintf(stderr,"ERROR; return code from lista(w_poczekalni) pthread_mutex_lock() is %d\n",errno);
	exit(EXIT_FAILURE);
    }
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
    if(pthread_mutex_unlock(&lista)!=0)
    {
	fprintf(stderr,"ERROR; return code from lista(w_poczekalni) pthread_mutex_unlock() is %d\n",errno);
	exit(EXIT_FAILURE);
    }
}
void dodaj_na_koniec_do_zrezygnowanych(struct lista **zrezygnowali, int klient)//dodanie klienta na koniec listy osob ktore zrezygnowalu
{
    if(pthread_mutex_lock(&lista)!=0)
    {
	fprintf(stderr,"ERROR; return code from lista(zrezygnowani) pthread_mutex_lock() is %d\n",errno);
	exit(EXIT_FAILURE);
    }
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
    if(debug==true)
    	zrezygnowali_wypisz();
    if(pthread_mutex_unlock(&lista)!=0)
    {
	fprintf(stderr,"ERROR; return code from lista(zrezygnowani) pthread_mutex_unlock() is %d\n",errno);
	exit(EXIT_FAILURE);
    }
}
int wez_pierwszego_i_go_zdejmij(struct lista **w_poczekalni)
{
	int pier;
	if(*w_poczekalni==NULL)
	{
		return -1;
	}
	else
	{
		pier=(*w_poczekalni)->id_klienta;
		if((*w_poczekalni)->next==NULL)
		{
			free(*w_poczekalni);
			(*w_poczekalni)=NULL;
		}
		else
		{
			struct lista *p=NULL;
			p=(*w_poczekalni)->next;
			free(*w_poczekalni);
			(*w_poczekalni)=p;
		}
		if(debug==true)
   	 		poczekalnia_wypisz();
		return pier;
	}	
}
void *Klient(void *numer_klienta)
{	
    int id=*(int *)numer_klienta;
    //usleep(1000);
    //klient w poczekalni wiec ja blokujemy
    if(pthread_mutex_lock(&poczekalnia)!=0)
    {
	fprintf(stderr,"ERROR; return code from poczekalnia pthread_mutex_lock() is w kliencie 1 %d\n",errno);
	exit(EXIT_FAILURE);
    }
    if(miejsca>0) //jesli sa wolne miejsca, wejdz
    {
        miejsca--;
	int bilet=bileciki_do_kontroli++;
        printf("Res:%d WRomm: %d/%d [in: %d]-wszedl do poczekalni\n",ilu_nie_weszlo,krzesla-miejsca,krzesla,id_obslugiwanego_klienta);
	dodaj_na_koniec_kolejki(&w_poczekalni,id);
	if(pthread_cond_signal(&obudzenie)!=0)//obudzenie fryzjera
	{
		fprintf(stderr,"ERROR; return code from obudzenie pthread_cond_signal() is w kliencie %d\n",errno);
		exit(EXIT_FAILURE);
	}
	//if(co_ile_nowy_klient==-1)
	//{
	//    losowy_czas_przychodzenia();
	//}
	//else sleep(co_ile_nowy_klient);
	while(bilet!=obecny_bilet)//sprawdzanie czy bilet naszego klienta zgadza siê z biletem klienta który ma byc teraz obsluzony 
	{
		if(pthread_cond_wait(&zapraszam_na_fotel,&poczekalnia)!=0)//czekanie na sygnal ze fryzjer zaprasza na fotel 
		{
			fprintf(stderr,"ERROR; return code from zapraszam_na_fotel pthread_cond_wait() is w kliencie %d\n",errno);
			exit(EXIT_FAILURE);
		}
	}
	obecny_bilet++;
	//printf("%d\n",id);
        //printf("Res:%d WRomm: %d/%d [in: %d]-wszedl do scinania\n",ilu_nie_weszlo,krzesla-miejsca,krzesla,id_obslugiwanego_klienta);
	if(pthread_mutex_unlock(&poczekalnia)!=0)//odblokowanie poczekalni
	{
		fprintf(stderr,"ERROR; return code from poczekalnia pthread_mutex_unlock() is w kliencie 3 %d\n",errno);
		exit(EXIT_FAILURE);
	}

    }
    else //brak wollnych miejsc w poczekalni
    {
        ilu_nie_weszlo++;
        printf("Res:%d WRomm: %d/%d [in: %d]  -klient nie wszedl\n",ilu_nie_weszlo,krzesla-miejsca,krzesla,id_obslugiwanego_klienta);
        dodaj_na_koniec_do_zrezygnowanych(&zrezygnowali,id);
	if(pthread_mutex_unlock(&poczekalnia)!=0) //odblokowanie poczekalni
	{
		fprintf(stderr,"ERROR; return code from poczekalnia pthread_mutex_unlock() is w kliencie 3 %d\n",errno);
		exit(EXIT_FAILURE);
	}
    }
    pthread_exit(NULL);
}
void *Fryzjer()
{
    while(1)
    {
    	if(pthread_mutex_lock(&poczekalnia)!=0)
    	{
		fprintf(stderr,"ERROR; return code from fryzjer_co_robi pthread_mutex_lock() is w fryzjerze %d\n",errno);
		exit(EXIT_FAILURE);
    	}
    	while(miejsca==krzesla) //jezeli poczekalnia jest pusta 
    	{
		if(pthread_cond_wait(&obudzenie, &poczekalnia)!=0)//czekanie na sygnal by wybudzenie fryzjera
    		{
			fprintf(stderr,"ERROR; return code from obudzenie pthread_cond_wait() is w fryzjerze %d\n",errno);
			exit(EXIT_FAILURE);
    		}
	}
	printf("Res:%d WRomm: %d/%d [in: %d]-wyszedl z poczekalni i idzie do fryzjera\n",ilu_nie_weszlo,krzesla-miejsca,krzesla,id_obslugiwanego_klienta);
	id_obslugiwanego_klienta=wez_pierwszego_i_go_zdejmij(&w_poczekalni); //zdjecie z kolejki id_klineta który ma byc ostrzyzony 
	miejsca++;//zwiekszenie liczby miejsc w poczekalni bo klient wyszedl z poczekalni
	printf("Res:%d WRomm: %d/%d [in: %d]-wszedl do scinania\n",ilu_nie_weszlo,krzesla-miejsca,krzesla,id_obslugiwanego_klienta);
	if(pthread_cond_broadcast(&zapraszam_na_fotel)!=0)//wys³anie sygnalu do ka¿dego w±tku 
	{
		fprintf(stderr,"ERROR; return code from zapraszam_na_fotel pthread_cond_broadcast() is w fryzjerze %d\n",errno);
		exit(EXIT_FAILURE);
	}
	if(pthread_mutex_unlock(&poczekalnia)!=0)//odblokowanie poczekalni 
    	{
		fprintf(stderr,"ERROR; return code from fryzjer_co_robi pthread_mutex_unlock() is w fryzjerze %d\n",errno);
		exit(EXIT_FAILURE);
	}
	if(fryzjer_strzyzenie==-1)//czas strzyzenia 
		losowy_czas_strzyzenia();
	else
		sleep(fryzjer_strzyzenie);
    }
    pthread_exit(NULL);
}
int main(int argc, char *argv[])
{
    //zainicjowanie zmiennych warunkowych i mutexow
    if(pthread_cond_init(&zapraszam_na_fotel,NULL)!=0)
    {
	fprintf(stderr,"ERROR; return code from zapraszam_na_fotel pthread_cond_init() is %d\n",errno);
	exit(EXIT_FAILURE);
    }
    if(pthread_cond_init(&obudzenie,NULL)!=0)
    {
	fprintf(stderr,"ERROR; return code from obudzenie pthread_cond_init() is %d\n",errno);
	exit(EXIT_FAILURE);
    }
    if(pthread_mutex_init(&lista,NULL)!=0)
    {
	fprintf(stderr,"ERROR; return code from fotel pthread_mutex_init() is %d\n",errno);
	exit(EXIT_FAILURE);
    }
    if(pthread_mutex_init(&poczekalnia,NULL)!=0)
    {
	fprintf(stderr,"ERROR; return code from poczekalnia pthread_mutex_init() is %d\n",errno);
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
    int tret[klienci];
    int sret;
    for(i=0;i<klienci;i++)
    {
        t[i]=i;
    }
    if(pthread_create(&fryzjer_watek,0,Fryzjer,0)!=0)
    {
	fprintf(stderr,"ERROR; return code from Fryzjer pthread_create() is %d\n",errno);
	exit(EXIT_FAILURE);
    }
    for(i=0;i<klienci;i++)
    {
	if(co_ile_nowy_klient==-1)
	{
	    losowy_czas_przychodzenia();
	}
	else sleep(co_ile_nowy_klient);
	if(pthread_create(&klient_watek[i],0,Klient,(void *)&t[i])!=0)
	{
		fprintf(stderr,"ERROR; return code from Klient pthread_create() is %d\n",errno);
		exit(EXIT_FAILURE);
	}
    }
    for(i=0;i<klienci;i++)
    {
        if(pthread_join(klient_watek[i],0)!=0)
	{
		fprintf(stderr,"ERROR; return code from klient_watek pthread_join() is %d\n",errno);
		exit(EXIT_FAILURE);
	}
    }
    koniec=true;
    //if(pthread_cond_signal(&obudzenie)!=0)
    //{
    //	fprintf(stderr,"ERROR; return code from obudzenie pthread_cond_signal() is %d\n",errno);
    //	exit(EXIT_FAILURE);
    //}
    if(pthread_join(fryzjer_watek, 0)!=0)
    {
	fprintf(stderr,"ERROR; return code from fryzjer_watek pthread_join() is %d\n",errno);
	exit(EXIT_FAILURE);
    }
    if(pthread_cond_destroy(&zapraszam_na_fotel)!=0)
    {
	fprintf(stderr,"ERROR; return code from zapraszam_na_fotel pthread_cond_destroy() is %d\n",errno);
	exit(EXIT_FAILURE);
    }
    if(pthread_cond_destroy(&obudzenie)!=0)
    {
	fprintf(stderr,"ERROR; return code from obudzenie pthread_cond_destroy() is %d\n",errno);
	exit(EXIT_FAILURE);
    }
    if(pthread_mutex_destroy(&lista)!=0)
    {
	fprintf(stderr,"ERROR; return code from fotel pthread_cond_destroy() is %d\n",errno);
	exit(EXIT_FAILURE);
    }
    if(pthread_mutex_destroy(&poczekalnia)!=0)
    {
	fprintf(stderr,"ERROR; return code from poczekalnia pthread_cond_destroy() is %d\n",errno);
	exit(EXIT_FAILURE);
    }
    free(zrezygnowali);
    free(w_poczekalni);
    return 0;
}






























