Checkpoint 1)

The current version allows a receiver to get multiple chunks rom multiple peers at the same time. We use the recv_from and send_to instead of the spiffy functions for ease of use. Both the sender and receiver maintain a "window," or a linked list of chunks currently being sent or received. For the reciever, each node is just a list of packets received with a last received pointer. The sender reads in data as its window slides. We handle the case where more than one chunk comes from a single peer by blocking until the current chunk has finished downloading. 

