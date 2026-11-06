import subprocess
import time

import cv2
import RPi.GPIO as GPIO
import smbus2
from picamera2 import Picamera2


I2C_ADDRESS = 0x50
FRAME_WIDTH = 640
FRAME_HEIGHT = 480
SHUTDOWN_PIN = 21
FACE_CASCADE_PATH = cv2.data.haarcascades + "haarcascade_frontalface_default.xml"

# Load the face detector and start the camera preview.
face_detector = cv2.CascadeClassifier(FACE_CASCADE_PATH)
picam2 = Picamera2()
picam2.configure(
	picam2.create_preview_configuration(
		main={"format": "XRGB8888", "size": (FRAME_WIDTH, FRAME_HEIGHT)}
	)
)
picam2.start()
time.sleep(0.1)

bus = smbus2.SMBus(1)
time.sleep(1)

# A pull-up keeps the shutdown input high until the button is pressed.
GPIO.setmode(GPIO.BCM)
GPIO.setup(SHUTDOWN_PIN, GPIO.IN, pull_up_down=GPIO.PUD_UP)

previous_center_x = 0
previous_center_y = 0


def send_axis_command(command, difference):
	"""Send a direction and magnitude using the Arduino's I2C byte protocol."""
	magnitude = abs(difference)
	first_byte = min(magnitude, 254)
	second_byte = max(magnitude - 254, 0)
	bus.write_byte_data(I2C_ADDRESS, command, first_byte)
	bus.write_byte_data(I2C_ADDRESS, second_byte, 255)


def send_axis_position(x_difference, y_difference):
	"""Send horizontal and vertical offsets; return True if I2C fails."""
	try:
		x_command = 1 if x_difference >= 0 else 3
		y_command = 2 if y_difference >= 0 else 4
		send_axis_command(x_command, x_difference)
		time.sleep(0.02)
		send_axis_command(y_command, y_difference)
		return False
	except IOError:
		return True


try:
	while True:
		image = picam2.capture_array()
		gray = cv2.cvtColor(image, cv2.COLOR_BGR2GRAY)
		faces = face_detector.detectMultiScale(gray, 1.1, 5)

		if len(faces) > 0:
			selected_index = min(
				range(len(faces)),
				key=lambda index: (
					(faces[index][0] + faces[index][2] // 2 - previous_center_x) ** 2
					+ (faces[index][1] + faces[index][3] // 2 - previous_center_y) ** 2
				),
			)

			for index, (x, y, width, height) in enumerate(faces):
				color = (0, 0, 255) if index == selected_index else (0, 255, 0)
				cv2.rectangle(image, (x, y), (x + width, y + height), color, 1)

			if selected_index is not None:
				x, y, width, height = faces[selected_index]
				center_x = int(x + width / 2)
				center_y = int(y + height / 2)
				cv2.rectangle(
					image,
					(center_x - 4, center_y - 4),
					(center_x + 4, center_y + 4),
					(0, 0, 255),
					1,
				)

				# Positive offsets mean right or below the image center.
				x_difference = center_x - FRAME_WIDTH // 2
				y_difference = center_y - FRAME_HEIGHT // 2
				previous_center_x = center_x
				previous_center_y = center_y

				if send_axis_position(x_difference, y_difference):
					cv2.putText(
						image,
						"I2C error: robot arm cannot follow",
						(50, 50),
						cv2.FONT_HERSHEY_SIMPLEX,
						0.8,
						(0, 0, 255),
						2,
					)

		image = cv2.resize(image, (1280, 960), interpolation=cv2.INTER_AREA)
		cv2.namedWindow("Camera", cv2.WINDOW_NORMAL)
		cv2.setWindowProperty("Camera", cv2.WND_PROP_FULLSCREEN, cv2.WINDOW_FULLSCREEN)
		cv2.imshow("Camera", image)

		if not GPIO.input(SHUTDOWN_PIN):
			subprocess.call(["shutdown", "-h", "now"])
			break

		cv2.waitKey(1)
finally:
	picam2.stop()
	GPIO.cleanup()
	cv2.destroyAllWindows()
