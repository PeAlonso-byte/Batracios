// Batracios.cpp : Este archivo contiene la función "main". La ejecución del programa comienza y termina ahí.
//
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include "stdio.h"
#include "ranas.h"

VOID f_Criar(int);
int mover_rana(int*, int*, int*);
DWORD WINAPI ranitasHijas(LPVOID);
HANDLE semaforo, Memoria, s_numRanas;
HANDLE semaforoMadres[4];
char errores[100];
HINSTANCE libreria;
BOOL(*iniciaRanas)(int, int*, int*, int*, int, TIPO_CRIAR);
VOID(*PrintMsg)(char*);
BOOL(*PartoRanas)(int);
BOOL(*PuedoSaltar)(int, int, int);
BOOL(*AvanceRanaIni)(int, int);
BOOL(*AvanceRana)(int*, int*, int);
BOOL(*AvanceRanaFin)(int, int);
BOOL(*AvanceTroncos)(int);
BOOL(*ComprobarEstadIsticas)(long, long, long);
FARPROC Pausa, FinRanas;


int* matrizRanas[34][2];

typedef struct estadisticas {
	int ranasNacidas;
	int ranasSalvadas;
	int ranasPerdidas;
} estadisticas;

int main(int argc, char* argv[])
{
	int ret, tCriar;
	int troncos[7] = { 3,3,3,3,3,3,3 };
	int laguas[7] = { 1,1,1,1,1,1,1 };
	int dirs[7] = { 0,1,1,0,1,0,0 };
	if (argc != 3) {
		sprintf(errores, "USO: batracios.exe tics tcria", argv[0]);
		perror(errores);
		return -1;
	}
	ret = atoi(argv[1]);
	tCriar = atoi(argv[2]);

	if (ret < 0 || ret > 1000 || tCriar <= 0) {
		sprintf(errores, "USO: PRIMER ARGUMENTO (0-1000) SEGUNDO ARGUMENTO > 0", argv[0]);
		perror(errores);
		return -1;
	}

	libreria = LoadLibrary("ranas.dll");
	if (libreria == NULL) {
		return GetLastError();
	}

	/* INICIALIZAMOS LOS VALORES DE LAS ESTADISTICAS */	
	estadisticas es;
	es.ranasNacidas		= 0;
	es.ranasPerdidas	= 0;
	es.ranasSalvadas	= 0;

	/* INICIALIZAMOS LA MATRIZ PARA SABER LAS POSICIONES DE CADA RANA */

	for (int i = 0; i < 34; i++)
		for (int j = 0; j < 2; j++)
			matrizRanas[i][j] = 0;

	/* INICIALIZAMOS LA MEMORIA COMPARTIDA PARA LOS CONTADORES. */

	Memoria = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(estadisticas), "Contadores");
	if (Memoria == NULL) {
		return GetLastError();
	}

	LPSTR puntero = (LPSTR)MapViewOfFile(Memoria, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(estadisticas));
	if (puntero == NULL) return GetLastError();

	CopyMemory(puntero, &es, sizeof(estadisticas));

	/* INICIALIZAMOS LAS FUNCIONES DE LA LIBRERIA */

	PrintMsg = (VOID(*)(char*)) GetProcAddress(libreria, "PrintMsg");
	if (PrintMsg == NULL) return GetLastError();

	PartoRanas = (BOOL(*)(int)) GetProcAddress(libreria, "PartoRanas");
	if (PartoRanas == NULL) return GetLastError();

	PuedoSaltar = (BOOL(*)(int, int, int)) GetProcAddress(libreria, "PuedoSaltar");
	if (PuedoSaltar == NULL) return GetLastError();

	AvanceRanaIni = (BOOL(*)(int, int)) GetProcAddress(libreria, "AvanceRanaIni");
	if (AvanceRanaIni == NULL) return GetLastError();

	AvanceRana = (BOOL(*)(int*, int*, int)) GetProcAddress(libreria, "AvanceRana");
	if (AvanceRana == NULL) return GetLastError();

	AvanceRanaFin = (BOOL(*)(int, int)) GetProcAddress(libreria, "AvanceRanaFin");
	if (AvanceRanaFin == NULL) return GetLastError();

	AvanceTroncos = (BOOL(*)(int)) GetProcAddress(libreria, "AvanceTroncos");
	if (AvanceTroncos == NULL) return GetLastError();

	ComprobarEstadIsticas = (BOOL(*)(long, long, long)) GetProcAddress(libreria, "ComprobarEstadIsticas");
	if (ComprobarEstadIsticas == NULL) return GetLastError();

	Pausa = GetProcAddress(libreria, "Pausa");
	if (Pausa == NULL) return GetLastError();

	FinRanas = GetProcAddress(libreria, "FinRanas");
	if (FinRanas == NULL) return GetLastError();

	/* INICIALIZAMOS LOS SEMAFOROS */
	for (int i = 0; i < 4; i++) {
		if (i == 0) semaforoMadres[i] = CreateSemaphore(NULL, 1, 1, "ranaMadre0");
		if (i == 1) semaforoMadres[i] = CreateSemaphore(NULL, 1, 1, "ranaMadre1");
		if (i == 2) semaforoMadres[i] = CreateSemaphore(NULL, 1, 1, "ranaMadre2");
		if (i == 3) semaforoMadres[i] = CreateSemaphore(NULL, 1, 1, "ranaMadre3");
	}

	s_numRanas = CreateSemaphore(NULL, 26, 26, "numeroRanas");
	if (s_numRanas == NULL) return GetLastError();
	// ReleaseSemaphore(semaforo, 1, NULL); // Signal
	// WaitForSingleObject(semaforo, INFINITE); // Wait
	 
	semaforo = CreateSemaphore(NULL, 1, 1, "AvanceRana");
	iniciaRanas = (BOOL(*)(int, int*, int*, int*, int, TIPO_CRIAR)) GetProcAddress(libreria, "InicioRanas");
	iniciaRanas(ret, troncos, laguas, dirs, tCriar, &f_Criar);
	getchar();
	FreeLibrary(libreria);

	// Liberar Tambien la memoria compartida.

	return 0;
}

VOID f_Criar(int numero) {
	 
	// Aqui se tiene que llamar a partoRanas, actualizar estadisticas y crear el hilo para la nueva rana.
	WaitForSingleObject(semaforoMadres[numero], INFINITE); // La madre espera a que la ranita pequeña avance
	if (PartoRanas(numero)) {
		WaitForSingleObject(s_numRanas, INFINITE);
		HANDLE hijo = CreateThread(NULL, NULL, ranitasHijas, LPVOID(numero), 0, NULL);
		if (hijo == NULL) {
			sprintf(errores, "Error al crear el hilo");
			PrintMsg(errores);
		}
		// Aqui tendrian que ir las ranas salvadas o perdidas.
	}
	else {
		ReleaseSemaphore(semaforoMadres[numero], 1, NULL);
	}
}

DWORD WINAPI ranitasHijas(LPVOID s) {
	int contador = 0;
	int num = int(s);
	int dx = 15 + (16 * num);
	int dy = 0;
	int flag = 0;
	for (;;) {
		WaitForSingleObject(semaforo, INFINITE);
		if (mover_rana(&dx, &dy, &contador) != 0) {		
			break;
		}
		else { // Hasta que la rana no se mueva no libera el semaforo.
			if (flag == 0) {
				ReleaseSemaphore(semaforoMadres[int(s)], 1, NULL); // Signal
				flag = 1;
			}
		}
		ReleaseSemaphore(semaforo, 1, NULL); // El semaforo para que las ranas avance siempre se libera.
	}
	ReleaseSemaphore(s_numRanas, 1, NULL);
	ReleaseSemaphore(semaforo, 1, NULL); // El semaforo para que las ranas avance siempre se libera.

	return 0;
}

int mover_rana(int* dx, int* dy, int* contador) {

	int aleatorio = rand() % 2;
	int aleatorio2 = 0;
	if (aleatorio != 1) {
		aleatorio2 = 1;
	}
	if (*contador == 7 || *contador == 8) return 1;
	if (PuedoSaltar(*dx, *dy, ARRIBA)) {
		AvanceRanaIni(*dx, *dy);
		AvanceRana(dx, dy, ARRIBA);
		Pausa();
		AvanceRanaFin(*dx, *dy);
		*contador = 0;
	}
	else if (PuedoSaltar(*dx, *dy, aleatorio)) {
		AvanceRanaIni(*dx, *dy);
		AvanceRana(dx, dy, aleatorio);
		Pausa();
		AvanceRanaFin(*dx, *dy);
		*contador += 2;
	}
	else if (PuedoSaltar(*dx, *dy, aleatorio2)) {
		AvanceRanaIni(*dx, *dy);
		AvanceRana(dx, dy, aleatorio2);
		Pausa();
		AvanceRanaFin(*dx, *dy);
		*contador += 3;
	}
	else {
		return 1;
	}

	return 0;
}
