# Before:
read 4-byte length
read message body
write response
repeat

# After:  
read as many bytes as possible into a buffer
while the buffer contains a complete request:
    parse one request
    generate one response
    remove/advance past consumed bytes
keep incomplete bytes for the next read


Creation of connection struct:
