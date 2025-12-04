# 🃏 Blackjack Distribuido

Este repositorio contiene tres proyectos desarrollados en el marco de la asignatura **Sistemas Distribuidos**, cada uno implementado con una tecnología diferente para gestionar la comunicación distribuida. El objetivo principal es desarrollar el juego de **Blackjack** aplicando distintos enfoques y posteriormente realizar un ejercicio de procesamiento distribuido con **MPI**.

---

## 📂 Estructura del Repositorio

```
blackjack-distribuido/
├── proyecto1-sockets/
│   └── Blackjack implementado con comunicación mediante sockets
│
├── proyecto2-serviciosweb/
│   └── Blackjack expuesto mediante servicios web (REST)
│
└── proyecto3-mpi/
    └── Filtrado de imágenes utilizando procesamiento distribuido con MPI
```

---

## 🧩 Proyecto 1: Blackjack con Sockets

En este módulo se implementa el juego de Blackjack usando **comunicación de bajo nivel** mediante sockets TCP.

### ✦ Características principales

* Protocolo propio de mensajes entre cliente y servidor.
* Comunicación síncrona con manejo manual de peticiones.
* Gestión de múltiples clientes.

### ✦ Objetivo

Comprender el funcionamiento de la comunicación punto a punto sin abstracciones adicionales.

---

## 🌐 Proyecto 2: Blackjack con Servicios Web (SOAP)

En este proyecto se expone la lógica del juego a través de un conjunto de endpoints REST, permitiendo jugar mediante peticiones HTTP.

### ✦ Características principales

* Servicio SOAP para gestionar partidas, jugadores y acciones.

* Intercambio de mensajes XML conforme al estándar SOAP.

* Separación clara entre lógica del juego y capa de comunicación.

### ✦ Objetivo

Aprender a diseñar e implementar servicios web basados en SOAP y WSDL.

---

## 🏎️ Proyecto 3: Filtrado de Imágenes con MPI

En esta práctica se abandona el juego de Blackjack para centrarse en el **procesamiento distribuido**, aplicando filtros de imagen mediante **MPI (Message Passing Interface)**.

### ✦ Características principales

* División de una imagen en fragmentos procesados por distintos nodos.
* Implementación de filtros paralelizados.
* Reensamblado del resultado final.

### ✦ Objetivo

Comprender el modelo de paso de mensajes en entornos paralelos y distribuidos.

---

## 📌 Requisitos

* C.
* Librerías estándar para sockets y HTTP.
* Entorno MPI para la práctica 3 (OpenMPI).

---

## ✨ Autor

Proyecto desarrollado como parte de la asignatura *Sistemas Distribuidos*.

Copyright (c) 2025 Fernando Chang Liu Zhang y Kaikai Wang
Todos los derechos reservados.
