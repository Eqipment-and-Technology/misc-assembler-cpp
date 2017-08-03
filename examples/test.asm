begin:
low andc 0
high andc 0
add 6h
high addc aah
low addc 55h
ext inca skc
# 55AAh
jmp begin
