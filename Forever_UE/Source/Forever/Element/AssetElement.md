# AssetElement.md

## 弹选项的触发盒
资产有脚本(`asset->GetScript()`非空)才在`Init`建触发盒(世界120x120x80cm，随根缩放反算)，玩家的`AForeverCharacter`进入时把`asset->GetOptions()`快照推给MeetOption；`RefreshOptions()`在`add_option/remove_option`后刷新；`EndPlay`撤掉屏幕上的选项(被捡走时引擎不一定补发End)。没有脚本不建盒。
