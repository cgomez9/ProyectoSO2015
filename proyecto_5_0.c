/* 
NOTA 

Prioridades: 0 - 5 
id_modulo: 0 - 6

id_modulo 0 = Tarjeta de red
id_modulo 1 = Servidor de impresion
id_modulo 2 = Demonio de compactacion
id_modulo 3 = Antivirus y Firewall
id_modulo 4 = Procesos que soliciten CPU
id_modulo 5 = Procesos que soliciten espacio en disco
id_modulo 6 = Procesos que envien y soliciten datos a traves de la tarjeta de red;

Los modulos del 0 - 3 tienen prioridad 0
Los modulos del 4 - 6 tienen prioridad 1 - 5
*/  

/* POLITICA DE PRIORIDADES SIN DESALOJO */

/* Librerias */
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <semaphore.h>
#include <stdlib.h>

/* Define */
#define N 10
#define MAX 20
#define MOD %
#define AND &&
#define OR ||
#define FALSE 0
#define TRUE 1
#define SIZE_MEMORY 5000
#define ERROR -1

/* Declaracion de semaforos */
sem_t mutex, sem_procesos[N], mutex2, mutexMemoryAccess;

/* Estructuras */
typedef int bool;

typedef struct
{
	int Buffer_In, Buffer_Out;
	sem_t Bloqueado;
	sem_t total;
	sem_t In, Out;
} Tarjeta_Red;

typedef struct /* Tabla de segmento */
{
    int base;
    int limit;
    int n;
}tds;

typedef struct 
{
	int numero_proceso;
	int prioridad;
	int id_modulo;
	tds *tdsProcess;
	int estatus; /* 0 bloqueado, 1 listo, 2 en espera, 3 finalizado  */
} PCB;

int memory[SIZE_MEMORY];
int bitmap[SIZE_MEMORY];
PCB pcb_procesos[N]; /* Cola de listos */

/* Procedimientos */
void *proceso_en_ejecucion(PCB *p);
void creador_procesos(pthread_t procesos[], PCB pcb_procesos[]);
void ordenamiento_por_prioridad(PCB pcb_procesos[]);
/* -------------------------------------------------------------- */
void Insertar_Peticion_Buffer(int *Buffer);
void Envio_Peticion(int *Buffer);
void *Eliminar_Peticion_Buffer(int *Buffer);
void Liberar_Buffer(int *Buffer);
void Solicitar_Tarjeta_Red(Tarjeta_Red *TR);
/* --------------------------------------------------------------- */
/* retorna puntero a tabla de segmento, si no se asigno, retorna nulo */
tds* allocate_memory(int request);
/* compacta la memoria */ 
void compact(); 
/* quita un proceso de la memoria */
void quitprocess(int id); 

/* --------------------------------------------------------------- */

int main(void)
{
	/* Variables */
	pthread_t procesos[N]; /* Cola para ejecucion */
	int i, j = 0, x;

	/* Semilla para el srand */
	srand(time(NULL));

	/* Inicializacion de los semaforos */
	sem_init(&mutex, 0, 1);
	sem_init(&mutex2, 0, 1);
	sem_init(&mutexMemoryAccess, 0, 1);

	/* Inicializacion para el gestor de memoria*/
    for(x = 0; x < N; x++)
    	pcb_procesos[x].tdsProcess = NULL;

    for(x = 0; x < SIZE_MEMORY; x++)
    {
         if(x < 24)
            bitmap[x] = memory[x] = 1; /* los primeros 24 kb estan usados por el SO */
         else
            bitmap[x] = memory[x] = 0;
    }

	/* Inicializacion del vector de semaforos */
	for (i = 0; i < N; i++)
			sem_init(&sem_procesos[i], 0, 1);

	/* Se crean los procesos */
	creador_procesos(procesos, pcb_procesos);
	
	/* Planificacion */
	printf("\e[92m\e[1m AVISO: Presione [ENTER] para continuar ->");
	getchar();

	/* Se ordenan por prioridad */
	ordenamiento_por_prioridad(pcb_procesos);

	/* Ejecucion de los hilos */
	while(j != N)
	{
		sem_post(&sem_procesos[j]);
		if(pthread_join(procesos[j], NULL))
			printf("\n \e[1m\e[91m ERROR: Error al ejecutar los hilos");
		j++;
	}

	/* Quitando de memoria los procesos finalizados */
	for (j = 0; i < N; j++)
	{
		if(pcb_procesos[j].estatus = 3)
			quitprocess(j);
	}

	printf("\e[0m");
	return 0;
}

/* -------------------------------------------------------------------------------------------------- */

void *proceso_en_ejecucion(PCB *p)
{
	Tarjeta_Red TR;
	
	TR.Buffer_In = 0;
	TR.Buffer_Out = 0;
	sem_init(&TR.Bloqueado, 0, 1);
	sem_init(&TR.In, 0, 1);
	sem_init(&TR.Out, 0, 1);
	sem_wait(&TR.Bloqueado);
	sem_wait(&TR.In);
	sem_wait(&TR.Out);

	sem_wait (&sem_procesos[p->numero_proceso]);
	if(p->id_modulo == 0)
	{
		printf("\e[1m\e[94m AVISO: Entra al proceso %d para su ejecucion\n", p->numero_proceso);
		printf("\e[93m\e[1m Prioridad del proceso %d es %d \n", p->numero_proceso, p->prioridad);
		printf("\e[93m\e[1m ID del proceso %d es %d \n", p->numero_proceso, p->id_modulo);
		Solicitar_Tarjeta_Red(&TR);
		pcb_procesos[p->numero_proceso].estatus = 3;
	}
	else
	{
		printf("\e[1m\e[94m AVISO: Entra al proceso %d para su ejecucion\n", p->numero_proceso);
		printf("\e[93m\e[1m Prioridad del proceso %d es %d \n", p->numero_proceso, p->prioridad);
		printf("\e[93m\e[1m ID del proceso %d es %d \n", p->numero_proceso, p->id_modulo);
		system("sleep 3.0");
		pcb_procesos[p->numero_proceso].estatus = 3;
	}

}

void creador_procesos(pthread_t procesos[], PCB pcb_procesos[])
{
	int i, kb, aux;
	tds *tdsP; /* tabla de segmentos */
	
	for(i = 0; i < N; i++)
	{
		aux = rand() % 2;
		
		if(aux == 0)
			kb = 8;
		else if(aux == 1)
			kb = 12;
		else
			kb = 16;

		tdsP = allocate_memory(kb);
		
		if(tdsP != NULL)
        {
        	pcb_procesos[i].numero_proceso = i;
			pcb_procesos[i].prioridad = rand() % 6; /* Prioridad va de 0 - 5 */
			pcb_procesos[i].tdsProcess = tdsP;
			pcb_procesos[i].estatus = 1;

			if(pcb_procesos[i].prioridad == 0)
				pcb_procesos[i].id_modulo = rand() % 3; /* id_modulo va de 0 - 3 */
			else
				pcb_procesos[i].id_modulo = 4 + rand() % 3; /* id_modulo va de 4 - 6 */					

			sem_wait(&sem_procesos[i]);
			if(pthread_create(&procesos[i], NULL, (void *) &proceso_en_ejecucion, (void *) &pcb_procesos[i] ))
				printf("\e[1m\e[91m ERROR: Creacion del hilo %d invalido\n", i);
			else	
			{	
				printf("\e[1m\e[94m AVISO: Proceso %d creado exitosamente\n", i);
				system("sleep 1.0");
			}
		}
		else
			printf("\e[1m\e[91m ERROR: No hay memoria para el proceso %d\n", i);
	}
}

void ordenamiento_por_prioridad(PCB pcb_procesos[N])
{
	int i, j;
	PCB aux;

	for(i = 0;i <= N; i++)
	{
		for(j = 0;j < N-1; j++)
		{
			if(pcb_procesos[j].prioridad > pcb_procesos[j+1].prioridad)
			{
				aux = pcb_procesos[j];
				pcb_procesos[j] = pcb_procesos[j+1];
				pcb_procesos[j+1] = aux;
			}
		}
	}
}

/* -------------------------------------------------------------------------------------------------- */

void Insertar_Peticion_Buffer(int *Buffer)
{
	*Buffer = (*Buffer + 1);
	printf("\nNuevo Paquete a Enviar...\n");
	system("sleep 1.0");
}

void Envio_Peticion(int *Buffer)
{
	if(*Buffer < MAX)
	{
		printf("\nPaquete Guardado en el Buffer...\n");
		system("sleep 1.0");
		Insertar_Peticion_Buffer(Buffer);
	}
	else
	{
		printf("\nBuffer LLeno...\n");
		system("sleep 2.0");
	}
}

void *Eliminar_Peticion_Buffer(int *Buffer) /* Parametro *Buffer */
{
	if(*Buffer > 0)
	{
		printf("\nLiberando Buffer...\n");
		system("sleep 1.0");
		Liberar_Buffer(Buffer);
	}
}

void Liberar_Buffer(int *Buffer) /* Parametro *Buffer */
{
	*Buffer = (*Buffer - 1);
	printf("\nPaquete enviado...\n");
	printf("\nBuffer Liberado...\n");
	system("sleep 1.0");
}

void Solicitar_Tarjeta_Red(Tarjeta_Red *TR)
{
	int Valor, i, Tam;

	/* Si el firewall no esta bloqueado puedo recibir y enviar de una vez... */
	if((sem_getvalue(&TR->Bloqueado, &Valor) == 0) AND (Valor == 1))	
	{
		//En caso de que el Firewall halla bloqueado el envio por medio de la Tarjeta de Red luego de ser desbloqueada 
		//debe liberar los buffers...//
		if(TR->Buffer_In > 0)	///En caso de que los Buffer esten llenos debe liberarlos...
		{
			Tam = TR->Buffer_In;
			for(i = 0; i < Tam; i++)
			{
				Eliminar_Peticion_Buffer(&TR->Buffer_In);
			}
		}
		if(TR->Buffer_Out > 0)	///En caso de que los Buffer esten llenos debe liberarlos...
		{
			Tam = TR->Buffer_Out;
			for(i = 0; i < Tam; i++)
			{
				Eliminar_Peticion_Buffer(&TR->Buffer_In);
			}
		}
		/* Si valor es igual 0, entonces esa es la señal de que es un paquete de entrada. */
		if((sem_getvalue(&TR->In, &Valor) == 0) AND (Valor == 0))	
		{
			printf("\n\t*Paquete de Entrada*\n");
			system("sleep 1.0");
			Envio_Peticion(&TR->Buffer_In);
			printf("\nCapacidad del Buffer Entrada ->: %d\n", TR->Buffer_In);
			system("sleep 1.0");
			Eliminar_Peticion_Buffer(&TR->Buffer_In);
			printf("\nCapacidad del Buffer Entrada ->: %d\n", TR->Buffer_In);
			system("sleep 1.0");
			sem_post(&TR->In);
		}

		/* Si valor es igual 0, entonces esa es la señal de que es un paquete de salida. */
		if((sem_getvalue(&TR->Out, &Valor) == 0) AND (Valor == 0))	
		{
			printf("\n\t*Paquete de Salida*\n");
			system("sleep 1.0");
			Envio_Peticion(&TR->Buffer_Out);
			printf("\nCapacidad del Buffer Salida ->: %d\n", TR->Buffer_Out);
			system("sleep 1.0");
			Eliminar_Peticion_Buffer(&TR->Buffer_Out);
			printf("\nCapacidad del Buffer Salida ->: %d\n", TR->Buffer_Out);
			system("sleep 1.0");
			sem_post(&TR->Out);
		}
	}
	else
	{
		/* En este caso el Firewall bloqueo la tarjeta de Red, pero el igual puede recibir los 
		paquetes pero no los envia...*/
		printf("\nFirewall Bloqueo el Envio y Recepcion de Datos...\n");	
		system("sleep 2.0");
		if((sem_getvalue(&TR->In, &Valor) == 0) AND (Valor == 0))
		{
			printf("\n\t*Paquete de Entrada*\n");
			system("sleep 1.0");
			Envio_Peticion(&TR->Buffer_In);
			printf("\nCapacidad del Buffer Entrada ->: %d\n", TR->Buffer_In);
			system("sleep 1.0");
			printf("\nNo se puede enviar porque el Firewall bloqueo la _Tarjeta de Red...\n");
			system("sleep 1.0");
			sem_post(&TR->In);
		}
		if((sem_getvalue(&TR->Out, &Valor) == 0) AND (Valor == 0))
		{
			printf("\n\t*Paquete de Salida*\n");
			system("sleep 1.0");
			Envio_Peticion(&TR->Buffer_Out);
			printf("\nCapacidad del Buffer Salida ->: %d\n", TR->Buffer_Out);
			system("sleep 1.0");
			printf("\nNo se puede enviar porque el Firewall bloqueo la _Tarjeta de Red...\n");
			system("sleep 1.0");
			sem_post(&TR->Out);
		}
	}
}

/* -------------------------------------------------------------------------------------------------- */

tds* allocate_memory(int request)
{
    int cont, i, j, start;
    tds *tdsProcess;

    i=24;//asigna desde el bit 24 porque los primeros espacios son ocupados por el S.O
    cont=0;//para contar los espacios libres continuos

    sem_wait(&mutexMemoryAccess);//bloque el acceso a memoria

    do//busca los espacios libres necesarios para el segmento
    {
        if(bitmap[i]==0)
            cont++;
        i++;
    }
    while((cont < request) && (i<SIZE_MEMORY));
    i--;

    if(cont==request)//si encontro los espacios necesarios
    {
        start=(i-request+1);

        //llena la tabla de segmento del proceso
        tdsProcess=(tds*)malloc(sizeof(tds));
        tdsProcess->base=start;
        tdsProcess->limit=request;
        tdsProcess->n=0;
        //fin llenado

        for(j=i; j>=start; j--)
            memory[j]=bitmap[j]=1;//llena el mapa de bits y la memoria
    }
    else
        tdsProcess=NULL;//retorna null si no hay espacio
    sem_post(&mutexMemoryAccess);//libera la memoria
    
    return tdsProcess;
}

void compact()
{
	int i, j, k, l;
	int found, exitCompact;

    found=FALSE;
    exitCompact=FALSE;

    sem_wait(&mutexMemoryAccess);//bloque a todos los accesos a memoria para poder compactar

    i=0;
    do //hacer mientras hay espacios vacios
    {
        //busacar en memoria el primer espacio vacio
        while(!found && i<SIZE_MEMORY)
        {
            if(bitmap[i]!=0)
                i++;
            else
                found=TRUE;
        }

        if(i<SIZE_MEMORY)//se encontro espacios vacios
        {
            //buscar donde termina el agujero
            j=i+1;
            found=FALSE;
            while(!found && j<SIZE_MEMORY)
            {
                if(bitmap[j]!=1)
                    j++;
                else
                    found=TRUE;
            }

            if(j<=SIZE_MEMORY)//encontro el primer espacio ocupado
            {
                //buscar a que proceso le pertenece el espacio de memoria ocupado donde termina el agujero
                k=0;
                found=FALSE;
                while(!found && k<N)
                {
                    if(pcb_procesos[k].tdsProcess!=NULL) //si hay apuntador a una tds entonces esta en memoria el proceso
                    {
                        if(pcb_procesos[k].tdsProcess->base==j)
                            found=TRUE;
                    }
                    k++;
                }
                k--;
                if(k<N && found)//encontro el proceso que hay que mover
                {
                    //mover proceso hacia el primer espacio de memoria vacio
                        //cambio la tds del proceso
                    pcb_procesos[k].tdsProcess->base=i;
                    //pcb_procesos[k].tdsProcess->limit se mantiene igual
                        //muevo el mapa de bits y la memoria
                    for(l=i; l<j; l++)
                    {
                        bitmap[l]=memory[l]=1;
                        bitmap[j-(i-l)]=memory[j-(i-l)]=0;
                    }
                }
                else
                   exitCompact=TRUE;
            }
            else
                exitCompact=TRUE;
        }
        else
            exitCompact=TRUE;
        i=j;
    }
    while(!exitCompact);

    sem_post(&mutexMemoryAccess);//levanta el semaforo
}

void quitprocess(int id)
{
	int i,band, fin, j;

	sem_wait(&mutexMemoryAccess);

	band= i = FALSE;
	if(id>=0 && id<N)
    {
        //busco en el vector de pcb para encontrar la base y limite del proceso
        while(i<N && !band)
        {
            if(pcb_procesos[i].numero_proceso == id)
                band = TRUE;
            else
                i = i +1;
        }
        if(i<N && band)
        {
            //verificar estatus para comprovar si termino
            if(pcb_procesos[i].estatus ==1)
            {
                //liberar la memoria de la tabla de segmentos y el mapa de bits
                fin= pcb_procesos[i].tdsProcess->limit+pcb_procesos[i].tdsProcess->base-1;
                for(j = pcb_procesos[i].tdsProcess->base  ;j<=fin ;j++)
                    memory[j]=bitmap[j]=0;
                //liberar la tabla de segmentos
                pcb_procesos[i].tdsProcess->base=0;
                pcb_procesos[i].tdsProcess->limit=0;
                free(pcb_procesos[i].tdsProcess);
                pcb_procesos[i].tdsProcess=NULL;
            }
        }
        else
            printf("\nno se encontro el proceso %d\n", id);//solo con fines demostrativos
    }

    sem_post(&mutexMemoryAccess);
}
