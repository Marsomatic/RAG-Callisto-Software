import RPi.GPIO as g
from time import sleep

def main():
	g.setmode(g.BOARD)
	g.setup(11, g.OUT)

	g.output(11, 0)
	print("callisto off")
	sleep(5)
	g.output(11, 1)
	g.cleanup()
	print("callisto on")

if __name__ == "__main__":
	main()
