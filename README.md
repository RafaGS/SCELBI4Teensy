# Teensy SCELBI Emulator

[**English Version**](#english-version) **| [Versión en Español](#versión-en-español)**

## English Version

More information at [Minibots](https://minibots.wordpress.com/2026/10/25/emulador-de-scelbi-8h-con-teensy-4-1/).

This project is a SCELBI computer emulator specifically adapted to run on the Teensy platform. We use the EmulatorKit emulator as our base, modifying it to suit the requirements of an embedded microcontroller environment and modern interfaces.

### Architectural and Platform Changes

* **Teensy Adaptation:** The original `scelbi.c` has been fully adapted to run on Teensy. A new `main.c` was created to handle the entry point, and the entire project has been migrated to PlatformIO.

* **Pseudo-Monitor ROM:** Although the original SCELBI does not feature a ROM (or a monitor ROM), we have adapted the pseudo-monitor ROM from EmulatorKit. This effectively replaces the need for physical front panel operations.

### Memory and Storage Emulation

* **Fixed RAM:** The system memory is fixed at 4 KB using `SCELBI_RAM`.

* **MicroSD Integration:** The Teensy's native microSD reader has been integrated. Programs can be easily loaded using the monitor's `l` command.

### Serial Monitor and I/O

* **Upgraded Console:** The serial console has been updated to run at 115200 baud.

* **Panel Simulation:** Monitor commands have been created or updated to simulate physical operations that would normally be performed from the front panel.

* **Software Serial (Bit-Banging):** Port 016 is connected to the Teensy's USB-serial output using a software serial transmission routine (bit-banging). This routine reconstructs the character from the individual bits written by the emulated program and delivers it to `Serial`.

* **Text Normalization:** CR/LF text normalization has been added in `scelbi_serial_text.c` to ensure that line endings from SCELBI programs render correctly on modern terminals.

### Quick Start Guide

* **Installation:** Compile and upload the firmware to your Teensy board using PlatformIO. Ensure your microSD card is formatted correctly and contains the programs you wish to load.

* **Operation:** Open a serial terminal at 115200 baud. Use the pseudo-monitor commands (like `l` for loading) to interact with the emulated SCELBI computer.

### Requirements

* **Hardware:** A Teensy development board (with microSD slot).

* **Software/Libraries:** PlatformIO or Arduino IDE configured for Teensyduino.

### License

This project is based on EmulatorKit. In accordance with its original license, this project is licensed under the **GNU General Public License v3.0**. See the `LICENSE` file for more details.

### Retrocomputing

See this system on the [timeline](https://sites.google.com/view/minibotsmuseum/timeline) of our [Retrocomputing Museum](https://sites.google.com/view/minibotsmuseum).

---

## Versión en Español

Más información en [Minibots](https://minibots.wordpress.com/2026/10/25/emulador-de-scelbi-8h-con-teensy-4-1/).

Este proyecto es un emulador del ordenador SCELBI, adaptado específicamente para funcionar sobre la plataforma Teensy. Utilizaremos como base el emulador EmulatorKit, el cual hemos adaptado para un entorno embebido.

### Cambios de Arquitectura y Plataforma

* **Adaptación a Teensy:** Se ha adaptado el archivo `scelbi.c` para Teensy, se ha creado un `main.c` y se ha migrado todo el entorno de desarrollo a PlatformIO.

* **Pseudomonitor ROM:** Aunque el SCELBI original no dispone de ROM (ni de monitor ROM), se ha adaptado el pseudomonitor ROM de EmulatorKit para suplir y simular las operaciones que habitualmente se harían con el panel físico.

### Emulación de Memoria y Cinta

* **RAM Fija:** Se fija la memoria del sistema a 4 KB mediante `SCELBI_RAM`.

* **Integración MicroSD:** Se ha integrado el lector de microSD nativo del Teensy. El comando de monitor `l` permite la carga directa de programas desde la tarjeta.

### Monitor Serie y Entradas/Salidas

* **Consola Actualizada:** Se actualiza la consola serie para funcionar a 115200 baudios.

* **Simulación de Panel:** Se han actualizado o creado comandos de monitor que simulan las operaciones directas desde el panel frontal.

* **Transmisión Serie por Software:** El puerto 016 está conectado a la salida USB-serial del Teensy mediante una rutina de transmisión serie por software (bit-banging). La rutina reconstruye el carácter a partir de los bits que escribe el programa emulado y lo entrega al puerto `Serial`.

* **Normalización de Texto:** Se ha añadido una normalización de texto CR/LF en `scelbi_serial_text.c` para que los finales de línea de los programas SCELBI se visualicen correctamente en los terminales actuales.

### Guía Rápida de Uso

* **Instalación:** Compila y sube el firmware a tu placa Teensy usando PlatformIO. Asegúrate de tener una tarjeta microSD preparada con los programas a cargar.

* **Operación:** Abre un terminal serie a 115200 baudios. Interactúa con el pseudomonitor utilizando los comandos disponibles (como `l` para cargar) para operar el sistema.

### Requisitos

* **Hardware:** Placa de desarrollo Teensy (con ranura microSD).

* **Entorno de desarrollo:** PlatformIO (recomendado) o Arduino IDE con Teensyduino.

### Licencia

Este proyecto está basado en EmulatorKit. En cumplimiento de su licencia original, este proyecto está licenciado bajo la **GNU General Public License v3.0**. Puedes consultar el archivo `LICENSE` para más detalles.

### Retroinformática

Véase este sistema en la [línea temporal](https://sites.google.com/view/minibotsmuseum/timeline) de nuestro [Museo de la Retroinformática](https://sites.google.com/view/minibotsmuseum).