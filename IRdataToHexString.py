# Copy-paste the data from a flipper zero .ir file below, will ouput a string of hex bytes that can be sent via serial bluetooth app in HEX mode
DecimalString = "6212 522 1605 527 1498 559 1493 578 579"

numberlist = DecimalString.split(" ")

for num in numberlist:
    print(f"{int(num):04x} ", end="")
