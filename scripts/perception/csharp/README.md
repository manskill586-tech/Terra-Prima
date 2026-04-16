# C# Perception Client (Godot .NET)

Use this folder after switching the project to Godot `.NET`.

## NuGet packages

```powershell
dotnet add package Grpc.Net.Client
dotnet add package Google.Protobuf
dotnet add package Grpc.Tools
```

## Expected flow

1. Collect `AudioEffectCapture` chunks from `mic` and `world`.
2. Send `AudioChunk` to the Python gRPC service.
3. Build `PerceptionContext` (`audio_events`, `vision_summary`, `timestamp_ms`).
4. Route it to `GameServer.send_perception_context_to_kim(player_id, context)`.

See `PerceptionGrpcClient.cs` for a starter scaffold.
