#include <WiFi.h>
#include <DHT.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "time.h"
#include <Web3.h>
#include <Contract.h>
#include "secrets.h"
const char* ssid = SECRET_SSID;
const char* password = SECRET_PASS;
#define DHTPIN 4     
#define DHTTYPE DHT11   
DHT dht(DHTPIN, DHTTYPE);
const int ledPin = 2;
const int buttonPin = 12;
// Configurazione IPFS
const String pinataJWT = SECRET_PINATA_JWT;
const char* pinataEndpoint = "https://api.pinata.cloud/pinning/pinJSONToIPFS";
// Server NTP per il Timestamp
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600; // Fuso orario italiano
const int   daylightOffset_sec = 3600; // Ora legale (+1 ora)
// Configurazione Ethereum
const char* indirizzoPubblico = "0x5D9C88BEE400E5daA9Fa7fe2A0D010F669363D00";
const char* indirizzoContratto = "0xad24C7b3b3Da19774914D9C5C5521daAC4AA7672";
const char* chiavePrivata = SECRET_ETH_PRIVATE_KEY;
const char* urlRPC = SECRET_RPC_URL;
volatile bool pulsantePremuto = false;

String estraiCID(String payload) {
  if (payload.startsWith("0x")) {
    payload = payload.substring(2);
  }
  if (payload.length() < 136) {
    return "";
  }
  String hexLength = payload.substring(72, 136);
  int cidLength = strtol(hexLength.c_str(), NULL, 16);
  // Calcola dove iniziano e finiscono i dati effettivi
  int startIdx = 136;
  int endIdx = startIdx + (cidLength * 2); // *2 perché ogni carattere ASCII sono 2 HEX
  if (payload.length() < endIdx) return "";
  String hexCID = payload.substring(startIdx, endIdx);
  // Converte la stringa esadecimale in caratteri ASCII
  String cid = "";
  for (int i = 0; i < hexCID.length(); i += 2) {
    String byteString = hexCID.substring(i, i + 2);
    char c = (char) strtol(byteString.c_str(), NULL, 16);
    cid += c;
  }
  return cid;
}

void leggiDatiDaIPFS(String cid) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Errore: WiFi disconnesso.");
    return;
  }
  WiFiClientSecure client;
  client.setInsecure(); 
  client.setHandshakeTimeout(30000); 
  HTTPClient http;
  String url = "https://dweb.link/ipfs/" + cid;
  Serial.println("Scaricamento dati da IPFS: " + url);
  if (!http.begin(client, url)) {
    Serial.println("Errore inizializzazione HTTP");
    return;
  }
  http.setTimeout(15000);
  const char * headerKeys[] = {"Location"};
  http.collectHeaders(headerKeys, 1);
  int httpResponseCode = http.GET();
  if (httpResponseCode == HTTP_CODE_MOVED_PERMANENTLY || httpResponseCode == HTTP_CODE_FOUND || httpResponseCode == 307 || httpResponseCode == 308) {
    String newUrl = http.header("Location");
    Serial.println("Server reindirizza a: " + newUrl);
    http.end(); 
    if (!http.begin(client, newUrl)) {
      Serial.println("Errore inizializzazione HTTP verso il nuovo URL");
      return;
    }
    httpResponseCode = http.GET(); 
  }
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.print("Codice HTTP: ");
    Serial.println(httpResponseCode);
    Serial.println("Risposta da IPFS:");
    Serial.println(response);
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, response);
    if (error) {
      Serial.print("Errore nel parsing del JSON da IPFS: ");
      Serial.println(error.c_str());
    } else {
      float temperatura = doc["temperature"];
      String unita = doc["unit"];
      long timestamp = doc["timestamp"];
      Serial.println("--- Dati Sensore Estratti ---");
      Serial.print("CID IPFS: "); Serial.println(cid);
      Serial.print("Temperatura: ");
      Serial.print(temperatura);
      Serial.println(" °" + unita);
      Serial.print("Timestamp: ");
      time_t time = (time_t)timestamp;
      struct tm * timeinfo = localtime(&time);
      char buffer[80];
      strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
      Serial.println(buffer);
      Serial.println("-----------------------------");
    }
  } else {
    Serial.print("Errore richiesta HTTP a IPFS (Codice): ");
    Serial.println(httpResponseCode);
  }
  http.end();
}
String getTransazione(String txHash) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Errore: WiFi disconnesso.");
    return "";
  }
  HTTPClient http;
  delay(100);
  Serial.println("Interrogazione della blockchain per la transazione...");
  http.begin(urlRPC); 
  http.addHeader("Content-Type", "application/json");
  JsonDocument doc;
  doc["jsonrpc"] = "2.0";
  doc["method"] = "eth_getTransactionByHash";
  JsonArray params = doc["params"].to<JsonArray>();
  params.add(txHash);
  doc["id"] = 1;
  String requestBody;
  serializeJson(doc, requestBody);
  int httpResponseCode = http.POST(requestBody);
  if (httpResponseCode > 0) {
    String response = http.getString();
    JsonDocument responseDoc; 
    DeserializationError error = deserializeJson(responseDoc, response);
    if (error) {
      Serial.print("Errore nel parsing JSON: ");
      Serial.println(error.c_str());
      http.end();
      return "";
    }
    JsonObject result = responseDoc["result"];
    if (result.isNull()) {
      Serial.println("Transazione non trovata o ancora in attesa (Pending) nella mempool.");
      return "";
    } else {
      Serial.println("--- Dettagli Transazione ---");
      const char* blockHex = result["blockNumber"];
      long blockDec = strtol(blockHex, NULL, 16);
      Serial.print("Blocco (Hex): "); Serial.print(blockHex);
      Serial.print(" -> (Decimale): "); Serial.println(blockDec);
      Serial.print("Mittente (From): "); 
      Serial.println(result["from"].as<const char*>());
      Serial.print("Destinatario (To): "); 
      Serial.println(result["to"].as<const char*>());
      Serial.print("Gas Fornito: "); 
      Serial.println(strtol(result["gas"].as<const char*>(), NULL, 16));
      Serial.print("Valore (Wei Hex): "); 
      Serial.println(result["value"].as<const char*>());
      Serial.print("Dati Payload (Input): "); 
      String inputData = result["input"].as<const char*>();
      Serial.println(inputData);
      Serial.println("----------------------------");
      return inputData;
    }
  } else {
    Serial.print("Errore nella richiesta HTTP: ");
    Serial.println(httpResponseCode);
    return "";
  }
  http.end();
  return "";
}
void IRAM_ATTR Click() {
  pulsantePremuto = true;
}
void stampaSaldoETH() {
  Web3 web3(11155111);
  std::string indirizzo = std::string(indirizzoPubblico);
  uint256_t saldoWei = web3.EthGetBalance(&indirizzo);
  uint64_t saldoWei64 = (uint64_t)saldoWei;
  double saldoETH = saldoWei64 / 1000000000000000000.0;
  Serial.print("Saldo ETH rimanente: ");
  Serial.print(saldoETH, 6);
  Serial.println(" ETH");
}
String inviaSuEthereum(String cid_IPFS) {
  Serial.println("Invio su Ethereum");
  Serial.println("Avvio transazione per il CID: " + cid_IPFS);
  Web3 web3(11155111); // Connessione alla rete Sepolia
  Contract contract(&web3, indirizzoContratto); // Indirizzo del contratto
  contract.SetPrivateKey(chiavePrivata); // Imposta la chiave privata per firmare la transazione
  //String firmaFunzione = "updateCID(string)";
  //String parametri = "[\"" + cid_IPFS + "\"]"; 
  Serial.println("Firma e invio transazione in corso...");
  std::string indirizzoStr = std::string(indirizzoPubblico); // Convertiamo l'indirizzo pubblico in std::string
  std::string indirizzoMittente = std::string(indirizzoPubblico);
  uint32_t nonce = web3.EthGetTransactionCount(&indirizzoMittente); // Il nonce è il numero di transazioni inviate dall'indirizzo
  std::string indirizzoDestinatario = std::string(indirizzoContratto); // Convertiamo l'indirizzo del contratto in std::string
  uint256_t valoreEth = 0;
  std::string datiPayload = "0x07cce946"; // Funzione updateCID(string) in formato Ascii HEX (4 byte)
  // offset stringa
  datiPayload += 
  "0000000000000000000000000000000000000000000000000000000000000020";
  // lunghezza CID
  String len = String(cid_IPFS.length(), HEX);
  while(len.length() < 64)
      len = "0" + len;
  datiPayload += len.c_str();
  // CID in ASCII HEX
  for(int i = 0; i < cid_IPFS.length(); i++) {
      char buffer[3];
      sprintf(buffer, "%02x", cid_IPFS[i]);
      datiPayload += buffer;
  }
  // padding finale
  while((datiPayload.length()-2) % 64 != 0)
      datiPayload += "00";
  string hashTransazione = contract.SendTransaction(
      web3.EthGetTransactionCount(&indirizzoStr),
      1000000000ULL,  // 1 Gwei
      150000,         // Gas Limit
      &indirizzoDestinatario,
      &valoreEth,
      &datiPayload
  );
  Serial.print("Transazione inviata! Hash (TxID): ");
  String rispostaGrezza = String(hashTransazione.c_str());
  int jsonStart = rispostaGrezza.indexOf('{');
  int jsonEnd = rispostaGrezza.lastIndexOf('}');
  if (jsonStart == -1 || jsonEnd == -1 || jsonEnd < jsonStart) {
    Serial.println("Errore: risposta SendTransaction non valida.");
    return "";
  }
  String rispostaJson = rispostaGrezza.substring(jsonStart, jsonEnd + 1);
  JsonDocument txDoc;
  DeserializationError err = deserializeJson(txDoc, rispostaJson);
  if (err) {
    Serial.print("Errore parsing risposta SendTransaction: ");
    Serial.println(err.c_str());
    return "";
  }
  String resultHash = txDoc["result"].as<String>();
  if (resultHash.length() == 0) {
    Serial.println("Errore: campo 'result' assente nella risposta SendTransaction.");
    return "";
  }
  Serial.println(resultHash);
  return resultHash;
}
void exe() {
  digitalWrite(ledPin, HIGH);
  delay(1000);
  float tempCelsius = dht.readTemperature();
  if (isnan(tempCelsius)) {
    Serial.println("Errore: Impossibile leggere dal sensore DHT!");
    digitalWrite(ledPin, LOW);
    delay(1000);
    return;
  }
  // Ottieni il timestamp attuale (secondi dal 1970)
  time_t now;
  time(&now);
  // Creiamo un documento JSON. Allocazione dinamica di 200 byte.
  JsonDocument doc; 
  // Struttura richiesta dalle API di Pinata
  JsonObject pinataContent = doc["pinataContent"].to<JsonObject>();
  pinataContent["temperature"] = tempCelsius;
  pinataContent["unit"] = "C";
  pinataContent["timestamp"] = now;
  JsonObject pinataMetadata = doc["pinataMetadata"].to<JsonObject>();
  pinataMetadata["name"] = "Lettura_ESP32"; // Nome del file su Pinata
  // Serializziamo il JSON in una stringa di testo
  String jsonString;
  serializeJson(doc, jsonString);
  Serial.println("JSON Creato:");
  Serial.println(jsonString);
  // Chiamata API verso Pinata/IPFS
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    Serial.println("Inviando a IPFS tramite Pinata...");
    http.begin(pinataEndpoint);
    http.addHeader("Content-Type", "application/json");
    // Autenticazione con Bearer Token
    http.addHeader("Authorization", "Bearer " + pinataJWT); 
    // Esegui la richiesta POST
    int httpResponseCode = http.POST(jsonString);
    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.print("Codice HTTP: ");
      Serial.println(httpResponseCode);
      Serial.println("Risposta da Pinata (Contiene il CID):");
      Serial.println(response);
      JsonDocument responseDoc;
      deserializeJson(responseDoc, response);
      String cid = responseDoc["IpfsHash"].as<String>();
      Serial.print("Salvato su IPFS! CID generato: ");
      Serial.println(cid);
      String resultSendTx = inviaSuEthereum(cid);
      Serial.println("Attendere 2 minuti per la propagazione della transazione sulla blockchain Ethereum...");
      delay(120000); // Attendere 2 minuti per la propagazione della transazione
      String inputData = getTransazione(resultSendTx);
      String estrattoCID = estraiCID(inputData);
      Serial.print("CID estratto dalla transazione Ethereum: ");
      Serial.println(estrattoCID);
      leggiDatiDaIPFS(estrattoCID);
    } else {
      Serial.print("Errore nella richiesta HTTP: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  } else {
    Serial.println("Errore: Wi-Fi disconnesso.");
  }
  digitalWrite(ledPin, LOW);
  pulsantePremuto = false;
}
void setup() {
  Serial.begin(115200);
  dht.begin(); 
  pinMode(ledPin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);
  Serial.print("Connessione a ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connesso!");
  // Sincronizzazione dell'orologio interno dell'ESP32 tramite server NTP internet
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("Ora sincronizzata con NTP.");
  stampaSaldoETH();
  attachInterrupt(digitalPinToInterrupt(buttonPin), Click, FALLING);
  Serial.println("Premere il pulsante per inviare la lettura del sensore DHT11 a IPFS e a Blockchain Ethereum");
  // String inputData = getTransazione("0x837f3105c4970f81941cd5f38f91322ebf9b3a8c826501771d95cff1306e6c44");
  // String estrattoCID = estraiCID(inputData);
  // Serial.print("CID estratto dalla transazione Ethereum: ");
  // Serial.println(estrattoCID);
  // leggiDatiDaIPFS(estrattoCID);
}
void loop() {
  if (pulsantePremuto) {
    Serial.println("Sistema in azione...");
    exe();
  }
}