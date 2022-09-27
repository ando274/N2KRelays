# N2KRelays
A Basic NMEA 2000 Switch Bank able to control 8 relays

NMEA 2000 PGN 127501 "Binary Status Report" and PGN 127502 "Binary Switch Control".

A switching device can also be called a “load controller” as they control the supply to a load. NMEA has defined two NMEA 2000 messages that were designed to control a set of loads (PGN 127502) and for the load controller to broadcast the controllers current load state i.e., on or off (PGN 127501).
N2kRelays uses the PGN 127501/PGN 127502 pair to control a set of eight relays which can control eight loads. The following is a description of the logic of N2kRelays.

•	On start-up / power-on:

  o	 all relays are set to the “off” state
  
  o	A PGN 127501 is transmitted, including the controller instance (default = 0) and the first 8 load states.
  
  o	A PGN 127501 with the current state is transmitted periodically (default = 10 sec.) to ensure devices on the network are aware N2kRelays is online and synced i.e., a     “heartbeat function”
  
•	On reception of a PGN 127502

  o	Checks the “instance field” to ensure the message is for this controller
  
  o	Processes the first 8 load fields to determine if there is a change of state of any of the relays required.
  
  o	Immediately transmits a 127501 to indicate the change has happened and any display device can display the current state
