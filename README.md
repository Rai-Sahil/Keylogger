# Keylogger
Stealthy Encrypted key logger
## Installation
- Install openssl@3
  ```
  brew install openssl@3
  ```
  
- Compile the code using GCC
  ```
  gcc -o main main.c -I/opt/homebrew/opt/openssl/include -L/opt/homebrew/opt/openssl/lib -lcrypto -lpthread -framework ApplicationServices
  ```

- Run the compiled file
  ```
  sudo ./main
  ```
