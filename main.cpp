#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Novice.h>
#include <imgui.h>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <chrono>

#include <cstdint>
#include <cstring>
#include <vector>

#pragma comment(lib, "Ws2_32.lib")

enum class MessageType : uint16_t {

	Text = 1,
	AssignPlayerId = 2, // サーバー＞クライアント
	JoinRequest = 3,    // クライアントー＞サーバ
	WorldState = 4,     // サーバー＞全参加者
	Disconnect = 5,
	PlayerInput = 6,

};

struct PlayerState {
	uint16_t id;
	uint16_t x;
	uint16_t y;
};

// Windowsアプリのmain関数。ImGuiを使うため、Debugで実行する。
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
	Novice::Initialize("NT1_01_03", 1280, 720);
	// キーボードの入力状態を保存
	char keys[256]{};

	char host[64] = "34.104.202.113";
	char port[8] = "8000";
	bool connected = false;

	char text[256] = "";
	char reply[512] = "";
	int sentBytesDisplay = 0;
	int receivedBytesDisplay = 0;

	// TODO 1: 通信用の変数を用意し、Winsockを初期化する。
	WSADATA wsaData{};
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	SOCKET clientSocket = INVALID_SOCKET;

	std::vector<PlayerState> players;

	// 送信できなかったデータを保存するバッファ
	std::vector<char> sendBuffer;

	// まだ処理できていない受信データを保存するバッファ
	std::vector<char> receiveBuffer;

	int gameLoopCount = 0;

	int fps = 0;
	int frameCount = 0;

	auto lastTime = std::chrono::steady_clock::now();

	// サーバーから割り当てられる自分のID
	uint16_t playerId = 0;

	while (Novice::ProcessMessage() == 0) {
		Novice::BeginFrame();
		// 現在のキーボード入力を取得
		Novice::GetHitKeyStateAll(keys);

		gameLoopCount++;

		
frameCount++;

		// 現在時刻を取得
		auto currentTime = std::chrono::steady_clock::now();

		// 前回計測から何秒経過したか
		double elapsed = std::chrono::duration<double>(currentTime - lastTime).count();

		// 1秒経過したらFPSを更新
		if (elapsed >= 1.0) {

			fps = static_cast<int>(frameCount / elapsed);

			frameCount = 0;
			lastTime = currentTime;
		}


#ifdef USE_IMGUI
		ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_Once);
		ImGui::SetNextWindowSize(ImVec2(600, 200), ImGuiCond_Once);
		ImGui::Begin("TCP Connection");
		ImGui::BeginDisabled(connected);
		ImGui::InputText("Host", host, sizeof(host));
		ImGui::InputText("Port", port, sizeof(port));
		if (ImGui::Button("Connect")) {
			// TODO 2: 接続先を取得し、ソケットを作って接続する。
			addrinfo hints{};
			hints.ai_family = AF_INET;       // IPv4
			hints.ai_socktype = SOCK_STREAM; // TCP向け
			addrinfo* address = nullptr;
			if (getaddrinfo(host, port, &hints, &address) == 0) {
				// 成功：address　が接続先情報を指す

				// TCP用のソケット
				clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

				// OS同氏が3回やり取りして接続する
				connected = connect(clientSocket, address->ai_addr, static_cast<int>(address->ai_addrlen)) == 0;

				if (connected) {

					// ノンブロッキングモードを有効にする
					u_long mode = 1;

					connected = ioctlsocket(clientSocket, FIONBIO, &mode) == 0;
				}

				// 終わったら情報を返し、失敗をしたら閉じる
				freeaddrinfo(address);
				if (!connected) {

					closesocket(clientSocket);
					clientSocket = INVALID_SOCKET;
				}

				if (connected) {

					// JoinRequestの本文長は0
					uint16_t payloadSize = 0;

					// 参加要求のメッセージ種別
					MessageType type = MessageType::JoinRequest;

					// ヘッダー4バイト
					char sendFrame[4]{};

					std::memcpy(sendFrame, &payloadSize, 2);
					std::memcpy(sendFrame + 2, &type, 2);

					// 送信バッファの末尾に4バイト追加
					sendBuffer.insert(sendBuffer.end(), sendFrame, sendFrame + 4);
				}
			}
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::BeginDisabled(!connected);

		ImGui::InputText("Text", text, sizeof(text));

		if (connected) {

			// 接続中ならキー入力を取得
			if (connected) {

				// 毎ループ0から作成する
				uint16_t inputBits = 0;

				// Wキー：上
				if (keys[DIK_W]) {
					inputBits |= 0x0001;
				}

				// Sキー：下
				if (keys[DIK_S]) {
					inputBits |= 0x0002;
				}

				// Aキー：左
				if (keys[DIK_A]) {
					inputBits |= 0x0004;
				}

				// Dキー：右
				if (keys[DIK_D]) {
					inputBits |= 0x0008;
				}

				// 本文は入力ビットの2バイト
				uint16_t payloadSize = 2;

				// メッセージの種類
				MessageType type = MessageType::PlayerInput;

				// ヘッダー4バイト + 本文2バイト
				char sendFrame[6]{};

				// 本文長
				std::memcpy(sendFrame, &payloadSize, 2);

				// メッセージ種別
				std::memcpy(sendFrame + 2, &type, 2);

				// WASDの入力情報
				std::memcpy(sendFrame + 4, &inputBits, 2);

				// 送信待ちの末尾へ追加
				sendBuffer.insert(sendBuffer.end(), sendFrame, sendFrame + 6);
			}

			// 送信待ちのデータがある場合
			if (connected && !sendBuffer.empty()) {

				// 送信を1回だけ試す
				int n = send(clientSocket, sendBuffer.data(), static_cast<int>(sendBuffer.size()), 0);

				if (n > 0) {

					// 送れたバイト数だけ先頭から削除
					sendBuffer.erase(sendBuffer.begin(), sendBuffer.begin() + n);

					sentBytesDisplay = n;
				} else if (n == 0) {

					connected = false;
				} else if (WSAGetLastError() != WSAEWOULDBLOCK) {

					connected = false;
				}
			}

			// 受信を1回だけ試す
			if (connected) {

				char chunk[2048]{};

				int n = recv(clientSocket, chunk, sizeof(chunk), 0);

				if (n > 0) {

					// 受信したデータを末尾に追加
					receiveBuffer.insert(receiveBuffer.end(), chunk, chunk + n);

					receivedBytesDisplay = n;
				} else if (n == 0) {

					// サーバーが切断した
					connected = false;
				} else if (WSAGetLastError() != WSAEWOULDBLOCK) {

					// 通信エラー
					connected = false;
				}
			}

			// 完成したフレームを順番に処理
			while (connected && receiveBuffer.size() >= 4) {

				// ヘッダーを読み取る
				uint16_t payloadSize = 0;
				MessageType type{};

				std::memcpy(&payloadSize, receiveBuffer.data(), 2);
				std::memcpy(&type, receiveBuffer.data() + 2, 2);

				// 本文が大きすぎる場合は切断
				if (payloadSize > 1024) {
					connected = false;
					break;
				}

				// ヘッダー4バイト + 本文長
				int frameSize = 4 + payloadSize;

				// まだ全部届いていなければ次のループまで残す
				if (static_cast<int>(receiveBuffer.size()) < frameSize) {
					break;
				}

				// 本文の先頭
				const char* payload = receiveBuffer.data() + 4;

				// 自分のIDを受け取る
				if (type == MessageType::AssignPlayerId && payloadSize == 2) {

					std::memcpy(&playerId, payload, 2);
				}

				// サーバーから切断通知
				if (type == MessageType::Disconnect) {

					connected = false;
					break;
				}

				// プレイヤー全員の位置を受信
				if (type == MessageType::WorldState && payloadSize >= 2) {

					uint16_t count = 0;
					std::memcpy(&count, payload, 2);

					// 人数とデータサイズを確認
					if (count <= 128 && payloadSize == 2 + count * 6) {

						players.resize(count);

						for (int i = 0; i < count; ++i) {

							std::memcpy(&players[i].id, payload + 2 + i * 6, 2);
							std::memcpy(&players[i].x, payload + 4 + i * 6, 2);
							std::memcpy(&players[i].y, payload + 6 + i * 6, 2);
						}
					}
				}

				// 処理したフレームを先頭から削除
				receiveBuffer.erase(receiveBuffer.begin(), receiveBuffer.begin() + frameSize);
			}
		}

		if (!connected && clientSocket != INVALID_SOCKET) {

			closesocket(clientSocket);
			clientSocket = INVALID_SOCKET;

			// 送受信バッファを空にする
			sendBuffer.clear();
			receiveBuffer.clear();

			// プレイヤー情報を初期化
			players.clear();
			playerId = 0;
		}

		if (ImGui::Button("Disconnect")) {

			connected = false;

			if (clientSocket != INVALID_SOCKET) {
				closesocket(clientSocket);
				clientSocket = INVALID_SOCKET;
			}

			// バッファとプレイヤー情報を初期化
			sendBuffer.clear();
			receiveBuffer.clear();
			players.clear();
			playerId = 0;
		}

		ImGui::Text("Sent: %d bytes", sentBytesDisplay);
		ImGui::Text("Received: %d bytes", receivedBytesDisplay);
		ImGui::Text("Reply: %s", reply);

		ImGui::EndDisabled();
		ImGui::Text("Status: %s", connected ? "Connected" : "Not connected");
		ImGui::Text("My Player ID: %u", static_cast<unsigned int>(playerId));
		ImGui::Text("Game loop: %d", gameLoopCount);
		ImGui::Text("FPS: %d", fps);
		ImGui::End();
#endif

		// サーバーから受信した全プレイヤーを描画
		for (const PlayerState& player : players) {

			// 自分ならオレンジ、他の人なら緑
			uint32_t color = player.id == playerId ? 0xFFAB40FF : 0x55BD79FF;

			Novice::DrawEllipse(player.x, player.y, 16, 16, 0.0f, color, kFillModeSolid);

			if (player.id == playerId) {

				// ゲームループに合わせて輪の半径を変更
				int ring = 20 + (gameLoopCount % 30);

				// 自分の円の周りに輪を描く
				Novice::DrawEllipse(player.x, player.y, ring, ring, 0.0f, 0xFFAB40FF, kFillModeWireFrame);
			}
		}

		Novice::EndFrame();
	}

	// TODO 4: 接続が残っていれば閉じ、Winsockを終了する。

	// 通信を使い終えるとき
	if (clientSocket != INVALID_SOCKET) {
		closesocket(clientSocket);
	}

	WSACleanup();

	Novice::Finalize();
	return 0;
}
