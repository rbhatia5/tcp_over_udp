all: reliable_sender reliable_receiver

reliable_sender: sender_main.c
	gcc -o reliable_sender sender_main.c

reliable_receiver: receiver_main.c
	gcc -o reliable_receiver receiver_main.c


clean:
	rm reliable_sender
	rm reliable_receiver
