#include <SPI.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

#define LOG_PERIOD 15000  //Период регистрации в миллисекундах, рекомендуемое значение 15000-60000.
#define MAX_PERIOD 60000  //Максимальный период регистрации.

unsigned long counts;     //кол-во импульсов
unsigned long cpm;        //кол-во импульсов за минуту
unsigned int multiplier;  //коэффициент для перевода кол-ва импульсов в кол-во импульсов за минуту
unsigned long previousMillis;  //переменная для определения времения начала сбора импульсов
float uSv;            // Переменная для отображения радиации в микроЗивертах
float ratio = 151.0; // Коофициент для перевода импульсов в микроЗиверты
float uP = 0; // Переменная для отображения радиации в микроРентгенах
const byte interruptPin = D5; // Порт ESP к которому подключен счетчик
const byte sw = D7; // Порт ESP к которому подключен счетчик

void ICACHE_RAM_ATTR tube_impulse();//все функции вызываемые по прерываниям теперь через атрибут ICACHE_RAM_ATTR

int times=0;
float indications=0;
int itSend=0;

String code     = "c09175e645"; //сюда свой код со страницы регистрации
const char* ssid     = "TYAN"; // название и пароль точки доступа
const char* password = "aezakmimi";
const char* host = "byradiation.herokuapp.com";//адрес сервера

void tube_impulse(){       //Функция подсчета имульсов
  counts++;
}

void setup(){             //функция инициализации
  counts = 0;
  cpm = 0;//устанавливаем начальные значения переменных
  multiplier = MAX_PERIOD / LOG_PERIOD;     //считаем коэффициент для перевода кол-ва импульсов в кол-во импульсов за минуту
  Serial.begin(9600);//указанием скорости для работы с последовательным портом
  interrupts();//разрешаем прерывания
  pinMode(sw, OUTPUT);//установка режима работы пина включения dc-dc преобразователя
  pinMode(interruptPin, INPUT);//установка режима работы пина
  attachInterrupt(digitalPinToInterrupt(interruptPin), tube_impulse, FALLING); //Определяем количество импульсов через внешнее прерывание на порту, порт ждет

   // Подключаемся к wifi
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("MAC address: ");
  Serial.println(WiFi.macAddress());
  Serial.println();
}

void loop(){//Основной цикл
  digitalWrite(sw, HIGH);//включаем dc-dc преобразователь
  unsigned long currentMillis = millis();//устанавливаем настоящее время
  if(currentMillis - previousMillis > LOG_PERIOD&times<4){ //смотрим по условию можно ли начинать обработку кликов, их подсчет происходит независимо, за счет прерываний
    previousMillis = currentMillis;//устанавливаем настоящее время
    cpm = counts * multiplier;//находим кол-во кликов в минуту
    Serial.println(cpm);//кол-во кликов в минуту
    uSv = cpm / ratio ;//переводим в микрозиверты в час
    Serial.println(uSv);//в микрозивертах в час
    uP = uSv * 100 ;//переводим в микрорентгены в час
    Serial.println(uP);//в микрорентгенах в час
    if(counts>0){
    counts = 0;//обнуляем кол-во кликов
    times++;//увеличиваем переменную раз замера
    }
    indications=indications+uP;//суммируем 4 значения в 4 периодах по 15 секунд, находим среднее
  }if(times==4){//если сняли показания 4 раза обрабатываем и отправляем данные
      digitalWrite(sw, LOW);//выключаем dc-dc преобразователь
    itSend=(int)(indications/4);//считаем среднее
        Serial.print("Радиационный фон");//выводим в монитор
        Serial.print(itSend);//выводим в монитор среднее что отправится
        Serial.println("мкР/ч");//выводим в монитор еденицы измерения
        WiFiClient client;//создаем объект клиент
        const int httpPort = 80;//объявляем порт хоста
        if (!client.connect(host, httpPort)) {//подключаемся к серверу
        Serial.println("connection failed");
        return;
        }
       //отправляем данные на сервер запросом GET
       String url = "/api/radiation?code="+code+"&radiation="+itSend;
       Serial.print("Requesting URL: ");
       Serial.println(url); // отображение запроса который будет сделан на сервер
       client.print(String("GET ") + url + " HTTP/1.1\r\n" + "Host: " + host + "\r\n" + "Connection: close\r\n\r\n");
       unsigned long timeout = millis();//даем
       while (client.available() == 0) {//10
       if (millis() - timeout > 10000)//секунд (хероку тупит очень)
       { Serial.println(">>> Client Timeout !");//на ответ серверу, иначе выводим сообчение
       client.stop(); return; } } // Чтение всех строк ответа от сервера, должна доходить строка true
       while (client.available())
       { String line = client.readStringUntil('\r'); Serial.print(line);//выводим ответ с сервера в монитор
       }
       Serial.println("closing connection");//отображаем сообщение о конце обмена запрос-ответ с сервером
       times=0;//обнуление переменных сколько раз мы сняли показания и их суммы замеров для подсчета среднего
       indications=0;
       Serial.println("Перерыв 30 минут");
       ESP.deepSleep(1000000*1800);  //1800 секунд=30 минут, каждые 30 минут собираем данные с станции
    }
}
