# DFSync (Distributed File Synchronization System)

## About

DFSync is a multi-server file management system that simulates a cloud-based distributed file system. Developed in C for Linux environments, DFSync allows users to upload, download, remove, and archive files using a single client interface. The system automatically routes files to appropriate back-end servers based on file type, while keeping the client experience centralized and simple.

## Key Features

### File Type-Based Routing
- `.c` files are stored on S1
- `.pdf` files are forwarded to S2
- `.txt` files are forwarded to S3
- `.zip` files are forwarded to S4

### System Features
- **Single Point of Communication:** Clients interact only with S1, which handles all communication with the other servers in the background.
- **Socket Programming:** Uses TCP/IP for reliable communication between client and server processes.
- **Multi-Client Support:** The main server (S1) handles multiple clients concurrently using fork().
- **Dynamic Directory Creation:** Automatically creates nested directories during file upload if the path does not exist.
- **Tar Archive Support:** Supports downloading all files of a given type as a .tar archive via the downltar command.

## Technologies Used
- **Programming Language:** C
- **Operating System:** Debian/Linux
- **Protocols:** TCP/IP
- **Bash Utilities:** fork, socket, stat, mkdir, tar, find

## Getting Started

### Prerequisites
- Debian/Linux operating system
- GCC compiler

### Installation
```bash
git clone https://github.com/yourusername/DFSync.git
cd DFSync
```

### Compilation
```bash
gcc -o s1 S1.c
gcc -o s2 S2.c
gcc -o s3 S3.c
gcc -o s4 S4.c
gcc -o client w25clients.c
```

### Usage

#### Run Each Server in a Separate Terminal
```bash
./s2    # Handles .pdf files
./s3    # Handles .txt files
./s4    # Handles .zip files
./s1    # Main routing server
```

#### Launch the Client
```bash
./client
```

## Project Structure
```
DFSync/
├── S1.c           # Main server
├── S2.c           # PDF server
├── S3.c           # TXT server
├── S4.c           # ZIP server
├── w25clients.c   # Client program
└── README.md      # Documentation
```

## Contributing
1. Fork the repository
2. Create a new branch: `git checkout -b feature-branch`
3. Make your changes and commit: `git commit -m "Add feature"`
4. Push to GitHub: `git push origin feature-branch`
5. Submit a pull request

## Contact
For questions or suggestions, please reach out at: [write2hnp@gmail.com](mailto:write2hnp@gmail.com)

**Feel free to adjust the content to better suit your project's details and structure.**
