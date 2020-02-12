import requests
import matplotlib.pyplot as plt
import math


URL = "http://192.168.25.34"

r = requests.get(URL)

parte_a, parte_b = r.text.split('MUDOU')
parte_a = parte_a.split(',')
parte_b = parte_b.split(',')
print('---------------------------------------------------------------------------------------------')
print(parte_a, len(parte_a))
print('---------------------------------------------------------------------------------------------')
print(parte_b, len(parte_b))


def rmsValue(arr):
    n = len(arr)
    square = 0.0
    
    for i in range(0, n):
        square += (float(arr[i])*float(arr[i]))

    mean = square/(float(n))

    return math.sqrt(mean)



y_rms = rmsValue(parte_b)
print('   ')
print(y_rms)

plt.plot(parte_a, parte_b)
plt.xticks(parte_a, " ")
plt.yticks(parte_b, " ")

plt.ylabel('Vin (V)')
plt.ylabel('t (us)')

plt.show()

