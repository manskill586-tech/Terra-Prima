# Python Perception Service

This folder contains a minimal gRPC service scaffold for sound-event routing.

## 1) Create environment

```powershell
cd "E:\my_projects\Terra Prima\scripts\perception\python_service"
python -m venv .venv
.\.venv\Scripts\Activate.ps1
pip install --upgrade pip
pip install -r requirements.txt
```

## 2) Generate gRPC stubs

```powershell
.\generate_stubs.ps1
```

## 3) Start service

```powershell
python server.py --host localhost --port 50123
```

`server.py` currently returns placeholder events. Replace `ClassifyChunk` logic with BEATs+CLAP inference.
If `127.0.0.1` bind fails on Windows/Python 3.13, use `localhost`.
If `50051` is occupied by another service, keep `50123` (default) or choose another free port.
