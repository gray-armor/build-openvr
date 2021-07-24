# RenderBlockの設計
## 3DモデルのRenderに必要な情報
- OpenGL関連の詳細な実装を抽象化
- とりあえずvertexdataarrayだけで描画できるようにするの方法を考える。
- 結局Renderingまでに必要な工程は何なのかを可視化する。

	/** Returns the transform from eye space to the head space. Eye space is the per-eye flavor of head
	* space that provides stereo disparity. Instead of Model * View * Projection the sequence is Model * View * Eye^-1 * Projection.
	* Normally View and Eye^-1 will be multiplied together and treated as View in your application.
	*/

- Model * View * (Head to Eye) *  HMDHead * Projection
