#Firmware für HBW-Sen-EP RS485 Modul:
 * Homematic Wired Hombrew Hardware Arduino nano als Homematic-Device 8-fach S0-Interface nach einer Vorlage von Thorsten Pferdekaemper  (thorsten@pferdekaemper.com) und Dirk Hoffmann (hoffmann@vmd-jena.de) 
 * mit Erweiterung um eine Identfy LED
 * mit Erweiterung um 3 Datenpunkte (Wattstunden, Impulse im Zeitintervall, Momentanleistung der letzten 5 Minuten)
 
===================================

#Hardwarebeschreibung:
 * Pinsettings for Arduino nano
 * D0: RXD, normaler Serieller Port fuer Debug-Zwecke und Firmware
 * D1: TXD, normaler Serieller Port fuer Debug-Zwecke und Firmware
 * D2: Direction (DE/-RE) Driver/Receiver Enable vom RS485-Treiber
 * D5: RXD, RO des RS485-Treiber
 * D6: TXD, DI des RS485-Treiber
 * D8: Button
 * D12: Identfy LED
 * D13: Button State LED
 * A1: Eingang 1
 * A0: Eingang 2
 * A3: Eingang 3
 * A2: Eingang 4
 * A4: Eingang 5
 * A5: Eingang 6
 * D3: Eingang 7
 * D7: Eingang 8
