# **HomeDefender IoT (an alarm system integrated with a camera)**

## **1. Introduction**
* In today's technological era, the Internet of Things (IoT) has been growing rapidly, being applied in various aspects of life and bringing numerous benefits and conveniences to people's lives. The development of devices to protect homes remotely via the Internet is no exception. 

* The HomeDefender product is an anti-theft device designed to protect homes by combining sensors, logical rules, and Internet connectivity. This allows homeowners to easily monitor and safeguard their homes anytime and anywhere. With its compact design, stability, and a simple, user-friendly website interface, the product promises to deliver an excellent and reassuring experience for users.

## **2. System Functionality Description**

* The device utilizes a **PIR** motion sensor to detect if someone approaches the house (the monitored area). Depending on the level of intrusion, the device will activate different modes:
	* **Safe Mode (Mode 1 – Safe System Mode):** The LED light remains blue if no intruders are detected.
	* **Alert Mode (Mode 2 – Alert System Mode):** The device will flash a red LED, and the buzzer will emit a "beep-beep-pause" sound pattern for 10 seconds. After 10 seconds, the system will return to Mode 1. If the intruder remains in the alert zone after this period, a notification will be sent to the user's email. This mode will repeat continuously until no further intrusion is detected.
	* **Emergency Alert Mode (Mode 3 – Emergency Warning System Mode):** In emergency situations, the user can activate the emergency alert mode via the website through the **Device Controls** page by pressing the “FORCE ALERT RIGHT NOW” button. The system will immediately emit a continuous buzzer sound and keep the red LED light on until this mode is deactivated. Simultaneously, the system will send a notification to the user's email.
* In addition, the device is equipped with a camera that can stream video anytime the user desires, allowing them to easily monitor and protect their home. The camera also has the capability to take photos automatically or manually, capturing images of intruders as evidence for reporting criminal activity.
* To ensure the captured photos are well-lit, the device includes a flash that can automatically turn on/off based on signals from a light sensor (the darker the environment, the brighter the flash). Alternatively, the user can manually control the flash through the website as a method of intrusion warning.
* The system stores the photos and records the number of alerts issued each day, along with specific timestamps. This feature helps users track and analyze the security situation in their area more effectively.
* If the system determines that the intruder poses a significant threat, an email will be sent to the user immediately to alert them of the potential danger to their home.
* Additionally, the device is connected to a website for easy use and management by the user. After logging in, the user can perform various functions such as:
	-   Adjusting the operating time.
	-   Turning the device on/off.
	-   Manually capturing photos.
	-   Streaming video.
	-   Viewing the alert history.
	-   Accessing stored photos.
* All of these actions can be done quickly and easily through the user-friendly web interface.

## **3. Members Distribution**
| Student ID | Full Name       | Tasks              |
|:----------:| --------------- | ------------------ |
|  22127322  | Lê Phước Phát   | Embedded Developer |
|  22127388  | Tô Quốc Thanh   | UI/UX Developer    |
|  22127441  | Thái Huyễn Tùng | Reporter           |

## **4. Video Demo**
<!-- BEGIN YOUTUBE-CARDS -->
[![HomeDefender IoT Demo](https://ytcards.demolab.com/?id=ZLpqiuz0C70&title=HomeDefender+IoT+Demo&lang=en&timestamp=ZLpqiuz0C70&background_color=%230d1117&title_color=%23ffffff&stats_color=%23dedede&max_title_lines=1&width=250&border_radius=5&duration=481 "HomeDefender IoT Demo")](https://www.youtube.com/watch?v=ZLpqiuz0C70)
<!-- END YOUTUBE-CARDS -->
