// Họ và Tên: Lê Phước Phát
// MSSV: 22127322
// Lớp: 22CLC10

const unsigned int LED_pins[3] = {8, 9, 10};
// Led_pins with blue led pin is 8, red led pin is 9, green led pin is 10.

const unsigned int trig_pin = 4;
const unsigned int echo_pin = 5;

const unsigned int buzzer_pin = 11;

const int threshold_1 = 50; // 50 cm threshold (min)
const int threshold_2 = 150; // 150 cm threshold (max)

const unsigned long led_delay_time = 500; // Đặt thời gian delay cho mỗi đèn led là 500 
const unsigned long buzzer_delay_time = 500; // Đặt thời gian delay cho loa buzzer là 500

long n_distance;

// Hàm đo khoảng cách từ cảm biến siêu âm
long get_distance(){
	digitalWrite(trig_pin, LOW); // Đặt chân trigger ở mức LOW để chuẩn bị đo
  	delayMicroseconds(2); // Đợi 2 micro giây để ổn định
  	digitalWrite(trig_pin, HIGH); // Đặt chân trigger ở mức HIGH để gửi xung
  	delayMicroseconds(10); // Đợi 10 micro giây để hoàn tất việc phát xung
  	digitalWrite(trig_pin, LOW); // Đặt lại chân trigger ở mức LOW
  	
  	long duration = pulseIn(echo_pin, HIGH); // Đo thời gian nhận tín hiệu phản hồi từ vật cản
  	long distance_cm = duration * 0.034 / 2; // Tính khoảng cách theo cm (âm thanh di chuyển với tốc độ 0.034 cm / Us)
  	return distance_cm; // Trả về khoảng cách tính được
}

// Khi xe ở vùng 1 thì LED sẽ sáng (tất cả link kiện còn lại sẽ ở trạng thái tắt).
void zone_1(){
  digitalWrite(LED_pins[1], LOW); // Tắt đèn led màu đỏ
  digitalWrite(LED_pins[2], LOW); // Tắt đèn led màu xanh lá
  digitalWrite(LED_pins[0], HIGH); // Tắt đèn led màu xanh dương
  noTone(buzzer_pin); // Tắt loa buzzer
}

// Khi xe vào vùng 2 thì LED 1 và LED 3 sẽ tắt, LED 2 bật sáng nhấp nháy liên tục và còi bắt đầu kêu (beep-pause-beep...)
void zone_2(){
  digitalWrite(LED_pins[0], LOW);
  digitalWrite(LED_pins[2], LOW);
  blink_one_led(LED_pins[1], led_delay_time);
  beep_buzzer(buzzer_pin, buzzer_delay_time);
}

// Khi xe vào vùng 3 thì LED 1 + LED 2 + LED 3 đồng loạt nhấp nháy liên tục và còi kêu (beep-pause-beep...)
// Tuy nhiên, khoảng cách xe càng gần với vật cản thì đèn càng nhấp nháy nhanh và còi càng kêu dồn dập.
void zone_3(){
  for(int i = 0; i < 3; ++i){
    digitalWrite(LED_pins[i], LOW);
  }
  int blink_rate = map(n_distance, 0, threshold_1, 100, 500);
  blink_all_leds(blink_rate);
  beep_buzzer(buzzer_pin, blink_rate);
}

// Hàm nhấp nháy một LED cụ thể với thời gian nhấp nháy cố định
void blink_one_led(int led_pin, long led_time){
  digitalWrite(led_pin, HIGH);
  delay(led_delay_time);
  digitalWrite(led_pin, LOW);
  delay(led_delay_time);
}

// Hàm nhấp nháy tất cả các LED với thời gian nhấp nháy cố định
void blink_all_leds(long led_time){
  for(int i = 0; i < 3; ++i){
   digitalWrite(LED_pins[i], HIGH);
  }
  delay(led_time);
  
  for(int i = 0; i < 3; ++i){
   digitalWrite(LED_pins[i], LOW); 
  }
  delay(led_time);  
}

// Hàm điều khiển còi báo động với thời gian điều chỉnh
void beep_buzzer(int buzzer_pin, int buzzer_delay_time){
  tone(buzzer_pin, 1000);
  delay(buzzer_delay_time);
  noTone(buzzer_pin);
  delay(buzzer_delay_time);
}

void setup()
{
  Serial.begin(9600);
  
  for (int i = 0; i < 3; ++i){
    pinMode(LED_pins[i], OUTPUT); // Set the led pins as output
  }
  
  pinMode(trig_pin, OUTPUT); // Set the trigger pin as output
  pinMode(echo_pin, INPUT); // Set the echo pin as input
  
  pinMode(buzzer_pin, OUTPUT); // Set the buzzer pin as output
  
  Serial.println("Starting ultrasonic sensor ...");
}	

void loop()
{
  // Đo khoảng cách từ cảm biến	và lưu vào n_distance
  n_distance = get_distance();
  Serial.print(n_distance);
  Serial.println(" cm");
  
  // Nếu khoảng cách lớn hơn ngưỡng 150cm (Vùng1)
  if (n_distance >= threshold_2){
    zone_1();
  }
  // Nếu khoảng cách nằm giữa 50 cm và 150 cm (Vùng 2)
  else if (n_distance > threshold_1 && n_distance < threshold_2){
  	zone_2();
  }
  // Nếu khoảng cách nhỏ hơn hoặc bằng 50 cm (vùng 3)
  else if (n_distance <= threshold_1){
    zone_3();
  }
}