# IoT Project (ESP32 + IPFS + Ethereum Sepolia)

Questo progetto implementa un sistema IoT basato su ESP32. L'obiettivo principale è dimostrare come sia possibile acquisire, memorizzare e leggere i dati di un sensore senza fare affidamento su server o database centralizzati di terze parti, sfruttando le tecnologie Web3.

I dati del sensore vengono salvati su **IPFS** (tramite Pinata) per uno storage distribuito, e il relativo CID (Content Identifier) viene registrato in modo immutabile su uno Smart Contract sulla blockchain di **Ethereum** (rete di test).

## Obiettivo
Mostrare l'integrazione end-to-end tra un nodo IoT e architetture decentralizzate (IPFS + Blockchain) per garantire l'immutabilità, la trasparenza e la disponibilità del dato.

## Flusso Esecutivo

Il sistema opera secondo il seguente flusso:
1. **Attivazione del sistema:** La pressione di un pulsante avvia il processo esecutivo: `exe()`.
2. **Acquisizione Dati:** L'ESP32 legge la temperatura dal sensore DHT11 e ottiene il timestamp esatto tramite il server NTP.
3. **Storage su IPFS:** I dati vengono serializzati in formato JSON e caricati sulla rete IPFS utilizzando l'API di Pinata che restituirà il **C**ontent **ID**entifier.
4. **Caricamento su Blockchain:** Viene generata e firmata localmente una transazione crittografica contenente il CID restituito da IPFS. Questa transazione viene poi trasmessa alla rete Ethereum Sepolia (chainID=11155111), la quale eseguirà il codice della funzione `updateCID(_nuovoCID)` dello Smart Contract per memorizzare il dato.
5. **Verifica:** Il sistema attende che il blocco si diffonda nella rete e interroga l'endpoint RPC per confermare il successo della transazione. Infine legge l'ultimo CID aggiornato dallo Smart Contract e scarica nuovamente il dato da IPFS per verifica.

## Dipendenze Software

Il sistema utilizza le seguenti librerie :
*   `WiFi.h` (Inclusa nel core ESP32)
*   `HTTPClient.h` e `WiFiClientSecure.h`
*   `DHT sensor library` di Adafruit
*   `ArduinoJson` di Benoit Blanchon
*   Libreria per Web3/Ethereum compatibile con ESP32 (per `<Web3.h>` e `<Contract.h>`)

## Configurazione (File `secrets.h`)

Per motivi di sicurezza, le credenziali e le chiavi crittografiche sono omesse dal codice sorgente principale. Crea un file denominato `secrets.h` nella stessa directory dello sketch e inserisci i tuoi parametri:

```cpp
#ifndef SECRETS_H
#define SECRETS_H

// Credenziali Wi-Fi
const char* SECRET_SSID = "IL_TUO_SSID";
const char* SECRET_PASS = "LA_TUA_PASSWORD";

// Credenziali Pinata
const String SECRET_PINATA_JWT = "IL_TUO_JWT_PINATA"; 
const char* SECRET_PINATA_ENDPOINT_SEND = "https://api.pinata.cloud/pinning/pinJSONToIPFS";
const char* SECRET_PINATA_ENDPOINT_READ = "IL_TUO_ENDPOINT_PINATA"
// Credenziali Ethereum Sepolia
const char* SECRET_ETH_PRIVATE_KEY = "LaTuaChiavePrivata";
const char* SECRET_RPC_URL = "https://eth-sepolia.g.alchemy.com/v2/TUO_PROJECT_ID";
// Indirizzi del contratto su Sepolia
const char* SECRET_CONTRACT_ADDRESS = "0xIndirizzoDelTuoSmartContract";
const char* PUBLIC_ADDRESS = "0xTuoIndirizzoPubblico";
const char* SECRET_CURRENT_CID_FUNCTION_ADDRESS = "0xCURRENT_CID_ADDRESS"; // Funzione currentCID() in formato Ascii HEX (4 byte)
const char* SECRET_UPDATE_CID_FUNCTION_ADDRESS = "0xUPDATE_CID_ADDRESS"; // Funzione updateCID(string) in formato Ascii HEX (4 byte)
#endif
```
## Configurazione (`Smart Contract`)

```solidity
pragma solidity ^0.8.0;

contract SensorData {
    string public currentCID;

    function updateCID(string memory _nuovoCID) public {
        currentCID = _nuovoCID;
    }
}
```