#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Novice.h>
#include <imgui.h>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>

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

	int gameLoopCount = 0;

	// サーバーから割り当てられる自分のID
	uint16_t playerId = 0;

	while (Novice::ProcessMessage() == 0) {
		Novice::BeginFrame();
		// 現在のキーボード入力を取得
		Novice::GetHitKeyStateAll(keys);

		gameLoopCount++;

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

				// 終わったら情報を返し、失敗をしたら閉じる
				freeaddrinfo(address);
				if (!connected) {

					closesocket(clientSocket);
					clientSocket = INVALID_SOCKET;
				}

				if (connected) {

					// 本文の長さは0バイト
					uint16_t payloadSize = 0;

					// 送信するメッセージの種類
					MessageType type = MessageType::JoinRequest;

					// 送信用の4バイト配列
					char sendFrame[4]{};

					// 先頭2バイトに本文長を格納
					std::memcpy(sendFrame, &payloadSize, 2);

					// 次の2バイトにメッセージ種別を格納
					std::memcpy(sendFrame + 2, &type, 2);

					// 4バイトすべてを送信する
					int sendBytes = 0;

					while (connected && sendBytes < 4) {

						int n = send(clientSocket, sendFrame + sendBytes, 4 - sendBytes, 0);

						if (n <= 0) {

							connected = false;
							break;
						}

						sendBytes += n;
					}
				}
			}
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::BeginDisabled(!connected);

		ImGui::InputText("Text", text, sizeof(text));

		if (connected) {

			// ヘッダー4バイト本文1024バイト
			char receiveFrame[1028]{};

			int receivedBytes = 0;

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

				
    // 送信するデータの合計サイズ
				int frameSize = 6;

				int sentBytes = 0;

				// 全部送るまで繰り返す
				while (connected && sentBytes < frameSize) {

					int n = send(clientSocket, sendFrame + sentBytes, frameSize - sentBytes, 0);

					// 送信失敗
					if (n <= 0) {
						connected = false;
						break;
					}

					sentBytes += n;
				}

				// 送信バイト数を表示
				sentBytesDisplay = sentBytes;


			}

			// ヘッダー4バイトを受信
			while (connected && receivedBytes < 4) {

				int n = recv(clientSocket, receiveFrame + receivedBytes, 4 - receivedBytes, 0);

				if (n <= 0) {

					connected = false;
					break;
				}

				receivedBytes += n;
			}

			// ヘッダーを読み取る
			if (connected) {

				uint16_t payloadSize = 0;
				MessageType type{};

				std::memcpy(&payloadSize, receiveFrame, 2);
				std::memcpy(&type, receiveFrame + 2, 2);

				// 本文が大きすぎる場合は切断
				if (payloadSize > 1024) {

					connected = false;
				}

				// 本文を受信する
				int bodyReceived = 0;

				while (connected && bodyReceived < payloadSize) {

					int n = recv(clientSocket, receiveFrame + 4 + bodyReceived, payloadSize - bodyReceived, 0);

					if (n <= 0) {

						connected = false;
						break;
					}

					bodyReceived += n;
				}

				// メッセージの種類を確認
				if (connected) {

					const char* payload = receiveFrame + 4;

					if (type == MessageType::AssignPlayerId && payloadSize == 2) {

						// サーバーから受け取った自分のIDを保存
						std::memcpy(&playerId, payload, 2);
					}

					if (type == MessageType::Disconnect) {

						connected = false;
					}

					if (type == MessageType::WorldState && payloadSize >= 2) {

						// 参加人数を取り出す
						uint16_t count = 0;
						std::memcpy(&count, payload, 2);

						// 人数とデータサイズが正しいか確認
						if (count <= 128 && payloadSize == 2 + count * 6) {

							// 人数分の保存領域を用意
							players.resize(count);

							// 一人ずつ情報を取り出す
							for (int i = 0; i < count; ++i) {

								// プレイヤーID
								std::memcpy(&players[i].id, payload + 2 + i * 6, 2);

								// X座標
								std::memcpy(&players[i].x, payload + 4 + i * 6, 2);

								// Y座標
								std::memcpy(&players[i].y, payload + 6 + i * 6, 2);
							}
						}
					}

					receivedBytesDisplay = 4 + bodyReceived;
				}
			}
		}

		if (!connected && clientSocket != INVALID_SOCKET) {

			// ソケットを閉じる
			closesocket(clientSocket);

			// 無効なソケットに戻す
			clientSocket = INVALID_SOCKET;

			// プレイヤーの一覧を空にする
			players.clear();
		}

		if (ImGui::Button("Disconnect")) {

			// 未接続状態に変更
			connected = false;

			// ソケットを閉じる
			if (clientSocket != INVALID_SOCKET) {
				closesocket(clientSocket);
				clientSocket = INVALID_SOCKET;
			}

			// プレイヤー一覧を削除
			players.clear();
		}

		ImGui::Text("Sent: %d bytes", sentBytesDisplay);
		ImGui::Text("Received: %d bytes", receivedBytesDisplay);
		ImGui::Text("Reply: %s", reply);

		ImGui::EndDisabled();
		ImGui::Text("Status: %s", connected ? "Connected" : "Not connected");
		ImGui::Text("My Player ID: %u", static_cast<unsigned int>(playerId));
		ImGui::Text("Game loop: %d", gameLoopCount);

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
