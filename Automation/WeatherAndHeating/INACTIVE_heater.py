import RPi.GPIO as g
from time import sleep

def main():
	g.setmode(g.BOARD)
	g.setup(21, g.OUT)

	g.output(21, 0)
	print("heater on")
	sleep(300)
	g.output(21, 1)
	g.cleanup()
	print("heater off")

if __name__ == "__main__":
	main()
