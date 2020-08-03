
/* meta-header: indicates which indicates which header fields are present in the packet
 * one-byte pieces that have continuation bits (1st bit of each byte
 * 1st byte: continuation bit, ESP, Version, Destination, Type, TTL, Flow
 * 2nd byte: continuation bit, source, length, checksum, don't fragment, reserved bit
 * 3rd byte: continuation bit, fragment offset, last fragment, 5 bits of reserved bits*/
