from __future__ import annotations

import argparse
import time
import uuid
from concurrent import futures

import grpc


def _import_generated():
    try:
        import perception_pb2  # type: ignore
        import perception_pb2_grpc  # type: ignore
        return perception_pb2, perception_pb2_grpc
    except ModuleNotFoundError as exc:
        raise RuntimeError(
            "gRPC stubs are missing. Run `generate_stubs.ps1` in this folder first."
        ) from exc


class AudioPerceptionServicer:
    def __init__(self, pb2, model_name: str) -> None:
        self._pb2 = pb2
        self._model_name = model_name
        self._last_error = ""

    def ClassifyChunk(self, request, context):
        # TODO: replace with BEATs+CLAP inference pipeline.
        source = request.source or "world"
        has_audio = len(request.pcm_s16le) > 0
        events = []
        if has_audio:
            events.append(
                self._pb2.AudioEvent(
                    event_id=f"evt_{uuid.uuid4().hex[:8]}",
                    label="unknown_sound",
                    confidence=0.3,
                    start_ms=0,
                    end_ms=200,
                    source=source,
                )
            )

        return self._pb2.AudioEventsReply(events=events, model_name=self._model_name)

    def Health(self, _request, _context):
        return self._pb2.HealthReply(
            service="audio_perception",
            status="SERVING",
            latency_ms=1,
            last_error=self._last_error,
        )


def serve(host: str, port: int, model_name: str) -> None:
    pb2, pb2_grpc = _import_generated()
    server = grpc.server(futures.ThreadPoolExecutor(max_workers=4))
    servicer = AudioPerceptionServicer(pb2, model_name=model_name)
    pb2_grpc.add_AudioPerceptionServicer_to_server(servicer, server)

    bind_host = host
    try:
        server.add_insecure_port(f"{host}:{port}")
    except RuntimeError as bind_error:
        # grpcio on some Windows setups (notably with Python 3.13) may fail on
        # explicit IPv4 host values but work with localhost resolution.
        if host in ("127.0.0.1", "0.0.0.0"):
            bind_host = "localhost"
            print(
                f"[PerceptionService] Bind failed for {host}:{port}, retrying with {bind_host}:{port} ..."
            )
            server.add_insecure_port(f"{bind_host}:{port}")
        else:
            raise bind_error

    server.start()
    print(f"[PerceptionService] Listening on {bind_host}:{port}")
    print(f"[PerceptionService] Model placeholder: {model_name}")
    try:
        while True:
            time.sleep(3600)
    except KeyboardInterrupt:
        print("\n[PerceptionService] Shutting down...")
        server.stop(grace=2)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Kim audio perception gRPC service")
    parser.add_argument("--host", default="localhost")
    parser.add_argument("--port", type=int, default=50123)
    parser.add_argument("--model-name", default="beats+clap-placeholder")
    return parser.parse_args()


if __name__ == "__main__":
    args = parse_args()
    try:
        serve(args.host, args.port, args.model_name)
    except RuntimeError as error:
        print(f"[PerceptionService] {error}")
        print(
            "Hint: from scripts/perception/python_service run .\\generate_stubs.ps1"
        )
