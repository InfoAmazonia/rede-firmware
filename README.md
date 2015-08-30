# Mãe d'Água - Firmware

*(Português)*

Este é o repositório que contém o código fonte do sensor de qualidade de água. O protótipo foi desenvolvido para Arduino Mega.

water-sensor é licenciado sob a MIT License, como apresentado no arquivo LICENSE e no cabeçalho do arquivo water-sensor.ino.

Os outros arquivos são licenciados de acordo com suas próprias licenças, de acordo com seus cabeçalhos.

*(English)*

This repository contains the source code of Mãe d'Água.

Mãe d'Água is an open hardware to monitor water quality in real time. The system will help infer physicochemical properties and variables that differentiate drinkable from contaminated water. Mãe d'Água is part of the project <a href="http://infoamazonia.org/projects/infoamazonia-network/">InfoAmazonia Network</a>.

The device will measure acidity in water by potential of hydrogen (pH), oxidation-reduction potential (ORP), conductivity, temperature and barometric pressure in the water level, which are important factors to consider when assessing water quality for human consumption.

The hardware is modular:
- Module 1 (M1): composed by Arduino Mega, it is responsible for all processing of information captured by the sensors and the entire power supply (connected to an external source).
- Module 2 (M2): Arduino shield, designed using Eagle platform, with the barometric pressure sensor and connectors for coupling the pH, ORP, conductivity and water temperature sensors. This way, it is possible to select modules that are appropriate aiming the best cost-benefit ratio of specific desired application.
- Module 3 (M3): responsible for mobile communication (2G), capable of transmitting data by Internet and by SMS to the InfoAmazonia Network internet server. As auxiliary data access strategy, the M2 also features a SD memory card slot.

The complete hardware will capture a reading of each of the sensors per hour, and send them to our remote server, which will provide visualization in an open database.

The development files of the project's hardware/software can be found at:
- Hardware's design: https://github.com/InfoAmazonia/rede-hardware
- Firmware: https://github.com/InfoAmazonia/rede-firmware
- Website for data visualization: https://github.com/InfoAmazonia/rede-site
- SMS backend (to receive data from the equipments and to send alerts to the communities): https://github.com/InfoAmazonia/rede-sms

Mãe d'Água is licensed under the MIT License, as presented in the LICENSE file and at the header of rede-firmware.ino.

The other files are licensed according to their own licenses, as per their headers.
