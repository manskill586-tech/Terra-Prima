// Starter scaffold for Godot .NET integration.
// This file is not compiled by the current GDScript-only setup.
// Use it after migrating the project to Godot .NET and generating C# stubs from perception.proto.

using System;
using System.Collections.Generic;
using Godot;

namespace TerraPrima.Perception
{
    public partial class PerceptionGrpcClient : Node
    {
        [Export] public string ServiceAddress = "http://127.0.0.1:50051";
        [Export] public NodePath GameServerPath;

        public override void _Ready()
        {
            GD.Print($"[PerceptionGrpcClient] Configure gRPC client for {ServiceAddress}");
        }

        public void RoutePerceptionContext(int playerId, Godot.Collections.Dictionary context)
        {
            var gameServer = GetNodeOrNull(NodePath.Empty.Equals(GameServerPath) ? "../GameServer" : GameServerPath);
            if (gameServer == null)
            {
                GD.PushWarning("[PerceptionGrpcClient] GameServer not found.");
                return;
            }

            if (!gameServer.HasMethod("send_perception_context_to_kim"))
            {
                GD.PushWarning("[PerceptionGrpcClient] GameServer has no send_perception_context_to_kim method.");
                return;
            }

            gameServer.Call("send_perception_context_to_kim", playerId, context);
        }
    }
}
