import RPi.GPIO as GPIO
import time

GPIO.setmode(GPIO.BCM)

ledPin = 18
buttonPin = 23

GPIO.setup(ledPin, GPIO.OUT)
GPIO.setup(buttonPin, GPIO.IN, pull_up_down=GPIO.PUD_UP)

def toggle(channel):
    if GPIO.input(channel):
	print("high!")
        GPIO.output(ledPin, GPIO.HIGH)
    else:
	print("low!")
        GPIO.output(ledPin, GPIO.LOW)

GPIO.add_event_detect(buttonPin, GPIO.BOTH, callback=toggle, bouncetime=300)

try:
    time.sleep(1000)
except KeyboardInterrupt:
    pass
finally:
    GPIO.cleanup()       # clean up GPIO on CTRL+C and normal exit  
