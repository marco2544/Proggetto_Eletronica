//libreria servomotore
#include <ESP32Servo.h>
//libreria Preferences (funziona per salvare le variabili in memoria)
#include <Preferences.h>
//Libreria standard per utilizzare i2c
#include <Wire.h>
//libreria accelerometro
#include <DFRobot_BMI160.h>

//struttura campionamento accelerometro
typedef struct {
  float x;
  float y;
  float z;
  unsigned long temp;
} Accelerazione;



//I/O

//creo l'oggetto per le preferences per salvare in memoria le variabili (funziona come una sorta di hasmap)
Preferences preferences;

//costanti  varie per la gestione degli input
const String EXIT = "ESC";
const String FINE = "FINE";


//SERVO

//creo l'oggetto servomotore (per dare i comandi)
Servo myServo;

//pin per il servo
const int pinServo = 14;

//matrice per i tempi espressi ms per i ms che il servo impiega a fare 1 grado a quella velocità
const float msPerDegreeTable[10] = { 1000, 20, 16, 12, 10, 8, 6, 5, 4, 3 };

//velocità impostate del servomotore
//                      1     2     3     4     5    6     7     8     9     10
const int pwmTable[22] = { 1400, 1300, 1200, 1100, 1000, 900, 800, 700, 600, 500,
                           1600, 1700, 1800, 1900, 2000, 2100, 2200, 2300, 2400, 2500,
                           //                     11    12    13    14    15    16    17    18    19    20
                           1566, 1435 };
//                    or    anor

//variabile per il servo memoriza la posizione del servomotore
float currentPosition = 0.0;

//ACCELEROMETRO

//creo l'oggetto accelerometro (per poter ricavare le informazioni da esso)
DFRobot_BMI160 bmi160;

//indirizzo seriale i2c
const int8_t i2c_addr = 0x69;

//numero campionamenti in una rotazione
const int N_CAMPIONAMENTI = 10;

// costante per la conversione in °/s
const float gyroScale = 2000.0 / 32768.0;

//variabili per l'accelerometro
int8_t rslt;
int16_t accelGyro[6] = { 0 };

//array per i dati dell'accelerometro
Accelerazione accelerezioni[N_CAMPIONAMENTI];






void setup() {
  //inizializza porta seriale
  Serial.begin(115200);
  delay(100);

  //controllo che l'accelerometro funziona
  if (bmi160.softReset() != BMI160_OK) {
    Serial.println("reset false");
    while (true)
      ;
  }

  if (bmi160.I2cInit(i2c_addr) != BMI160_OK) {
    Serial.println("init false");
    while (true)
      ;
  }

  //inizializza il servomotore con ([pin_del_servo], [velocità_minima], [velocità_massima])
  myServo.attach(pinServo, 500, 2500);
  //nizializza memoria
  currentPosition = preferences.getFloat("position", 0.0);
}


void loop() {

  //aspetto 3 secondi da un esecuzione più che altro perchè altrimenti all'avvio non stampa il menu
  delay(3000);

  // Legge la riga fino a INVIO
  String comando = leggi_input_tastiera("Inserisci (1)-inputServo (2)-CALIBRA (3)-VAI IN HOME");
  //converte in intero il comando scelto
  int scelta = comando.toInt();


  //debug
  /*
  Serial.print("selezione: ");
  Serial.print(scelta);
  Serial.print(" inserito: ");
  Serial.println(comando);
  */


  comando.clear();
  switch (scelta) {
    case 1:
      inputServo();
      stampaInfo();
      break;

    case 2:
      //calibraServo la posizione 0
      calibraServo();
      break;

    case 3:
      //faccio in modo che l'utente possa resettare la posizione del braccio
      rth();
      break;

    default:

      Serial.println("Valore non valido");
      break;
  }
}

//FUNZIONI I/O

String leggi_input_tastiera(String messaggio) {
  Serial.println(messaggio);
  Serial.print("> ");

  while (true) {
    if (Serial.available() > 0) {
      String input = Serial.readStringUntil('\n');
      input.trim();  // rimuove spazi e newline
      if (input.length() > 0) {
        return input;
      }
    }
  }
}

void stampa_errore() {
  Serial.println("Formato errato");
}

void stampaAccelerazioni() {
  for (int i = 0; i < N_CAMPIONAMENTI; i++) {
    Serial.print("lettura: ");
    Serial.print(i);
    stampaStruct(accelerezioni[i]);
  }
}

void stampaStruct(Accelerazione a) {
  Serial.print("X: ");
  Serial.print(a.x);
  Serial.print(" °/s  ");
  Serial.print("Y: ");
  Serial.print(a.y);
  Serial.print(" °/s  ");
  Serial.print("Z: ");
  Serial.print(a.z);
  Serial.print(" °/s  ");
  Serial.print("temp:");
  Serial.print(a.temp);
  Serial.println(" ms ");
}

void stampaMax(Accelerazione max[3]) {
  Serial.println("\nmax x:");
  stampaStruct(max[0]);

  Serial.println("max y:");
  stampaStruct(max[1]);

  Serial.println("max z:");
  stampaStruct(max[2]);
}

void stampaMin(Accelerazione min[3]) {
  Serial.println("\n min x:");
  stampaStruct(min[0]);  //stampa la struct di max rispetto x
  Serial.println("min y:");
  stampaStruct(min[1]);  //stampa la struct di max rispetto y
  Serial.println("min z:");
  stampaStruct(min[2]);  //stampa la struct di max rispetto z
}

void stampaInfo() {
  max();
  min();
  media();
  //da chiedere se la vuole visualizare
  stampaAccelerazioni();
}


//FUNZIONI MATEMATICHE/FORMULE

void max() {
  Accelerazione max[3] = { 0 };

  for (Accelerazione a : accelerezioni) {
    if (fabs(max[0].x) < fabs(a.x)) {
      max[0] = a;
    }
    if (fabs(max[1].y) < fabs(a.y)) {
      max[1] = a;
    }
    if (fabs(max[2].z) < fabs(a.z)) {
      max[2] = a;
    }
  }

  // chiamata al metodo di stampa il massimo
  stampaMax(max);
}
void min() {
  Accelerazione min[3] = { accelerezioni[0], accelerezioni[0], accelerezioni[0] };
  for (Accelerazione a : accelerezioni) {
    if (fabs(min[0].x) > fabs(a.x)) {
      min[0] = a;
    }
    if (fabs(min[1].y) > fabs(a.y)) {
      min[1] = a;
    }
    if (fabs(min[2].z) > fabs(a.z)) {
      min[2] = a;
    }
  }

  // chiamata al metodo di stampa il minimo
  stampaMin(min);
}

void media() {
  Accelerazione media = { 0 };
  for (Accelerazione a : accelerezioni) {
    media.x += a.x;
    media.y += a.y;
    media.z += a.z;
    media.temp = a.temp;
  }
  media.x = media.x / N_CAMPIONAMENTI;
  media.y = media.y / N_CAMPIONAMENTI;
  media.z = media.z / N_CAMPIONAMENTI;
  stampaStruct(media);
}


//FUNZIONI SERVO

void inputServo() {
  while (true) {
    //prendo in input di quanti gradi devo ruotare e la velocità
    String comando = leggi_input_tastiera("Inserisci [gradi] [velocita (1-10)] e premi INVIO (es. 90 5)  scrivi ESC per uscire:");

    //se l'utente scrive ESC torno al menu principale
    if (comando.equalsIgnoreCase(EXIT)) {
      comando.clear();
      return;
    }

    pulisciInput(comando);
    //pulisco il comando
    comando.clear();
  }
}

void calibraServo() {
  while (true) {
    //chiedo all'utente in che verso devo muovermi e chiemo imposta 0
    String comando = leggi_input_tastiera("(+)Orario (-)antiorario (FINE)imposta lo 0");
    if (comando.equalsIgnoreCase(FINE)) {
      currentPosition = 0;
      preferences.putFloat("position", currentPosition);
      return;
    }
    impostaZero(comando);
    comando.clear();
  }
}

void rth() {
  //torno alla posizione 0
  ruota(-(currentPosition), 20);
}

void impostaZero(String verso) {
  //in base al verso mi muovo di tot° per dare agio di ripristinadre lo 0
  if (verso.equalsIgnoreCase("+")) {
    ruota(5, 1);
  } else if (verso.equalsIgnoreCase("-")) {
    ruota(-5, 1);
  }
}

//ripulisce input dei dati per il servo e chiama [ruota] che muove il servo
void pulisciInput(String dati) {
  //debug
  //Serial.println(dati);

  //controllo comando non sia vuoto
  if (dati.length() == 0) {
    return;
  }

  // Divide input in gradi e velocità
  int spazzi = dati.indexOf(' ');

  //che ci sia uno spazio tra uno e l'altro
  if (spazzi == -1) {
    dati.clear();
    //funzione che stampa stampa_errore
    stampa_errore();
    return;
  }

  //divido la stringa per i vari dati ([gradi] [velocità])
  int degrees = dati.substring(0, spazzi).toInt();
  int vel = dati.substring(spazzi + 1).toInt();

  //controllo che la velocità abbia il giusto range
  if ((vel < 1 || vel > 10)) {
    dati.clear();
    //funzione che stampa stampa_errore
    stampa_errore();
    return;
  }

  // Muove il servo
  ruota(degrees, vel);

  //stampo i gradi e la velocità
  //debug
  /*
  Serial.print("Gradi: ");
  Serial.print(degrees);
  Serial.print(" Velocita: ");
  Serial.println(vel);

  //stampa posizione salvata in memoria
  Serial.print("Nuova posizione salvata: ");
  Serial.print(currentPosition);
  Serial.println(" gradi");
  */
}

void posizioneCorrente(int degrees) {
  //aggiorna la posizione corrente del servo
  currentPosition += degrees;

  //in caso faccio un giro completo azzero
  while (currentPosition > 360) {
    currentPosition -= 360;
  }
  while (currentPosition < -360) {
    currentPosition += 360;
  }

  // salva in memoria
  preferences.putFloat("position", currentPosition);
}

void ruota(int degrees, int speed) {
  int velDiRotazione = 0;
  int gradi = abs(degrees);  //perchè non è il senso movimento non è in base ai gradi ma alla veloctà
  speed = speed - 1;         //perche la mat parte da 0

  //in base ai gradi dico se senso orario o antiorario
  if (speed == 20) {
    if (degrees > 0) {
      velDiRotazione = 20;
    } else {
      velDiRotazione = 21;
    }
  } else if (degrees > 0) {
    velDiRotazione = speed + 10;
  } else {
    velDiRotazione = speed;
  }

  //ricavo tempo del movimento e velocità
  float msPerDeg = msPerDegreeTable[speed];
  float totalTime = msPerDeg * gradi;

  //avvio la rotazione e acquisisco i dati dell'accelerometro
  int i = 0;
  myServo.writeMicroseconds(pwmTable[velDiRotazione]);
  unsigned long stop = millis() + totalTime;
  unsigned long intervallo = 0;
  unsigned long start = millis();
  float campionamento = totalTime / N_CAMPIONAMENTI;
  while (millis() < stop) {
    intervallo = millis();
    acquisisciDatiAccelerometr(i, intervallo - start);
    i++;
    while (intervallo + campionamento > millis()) {
      //aspetto per un giusto camionamento
    }
  }

  // Ferma il servo
  myServo.writeMicroseconds(1500);
  //stampaInfo();



  //debug
  /*
  posizioneCorrente(degrees);
  Serial.print("Gradi: ");
  Serial.print(gradi);
  Serial.print(" Velocita: ");
  Serial.println(pwmTable[velDiRotazione]);
  Serial.print("POSIZIONE: ");
  Serial.print(currentPosition);
  */
}


//FUNZIONI ACCELEROMETRO

void acquisisciDatiAccelerometr(int i, float time) {
  //acqioisisco i dati dell'accelerometro nel mio array
  bmi160.getAccelGyroData(accelGyro);

  //in base alla posizione dell'array prendo vel x,y,z di accelerometro e giroscopio in mm/s^2
  float gx = accelGyro[0] * gyroScale;
  float gy = accelGyro[1] * gyroScale;
  float gz = accelGyro[2] * gyroScale;
  accelerezioni[i].x = accelGyro[0] * gyroScale;
  accelerezioni[i].y = accelGyro[1] * gyroScale;
  accelerezioni[i].z = accelGyro[2] * gyroScale;
  accelerezioni[i].temp = time;

  // stampo val giroscopio in °/s
  //debug
  /*
  Serial.print("Gyro X: "); Serial.print(gx); Serial.print(" °/s  ");
  Serial.print("Y: "); Serial.print(gy); Serial.print(" °/s  ");
  Serial.print("Z: "); Serial.println(gz);
  */
}
