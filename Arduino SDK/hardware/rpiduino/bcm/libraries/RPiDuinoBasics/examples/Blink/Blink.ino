void setup()  // Run once when the sketch starts. / 草稿啟動時執行一次。
{
    pinMode(LED_BUILTIN, OUTPUT);  // Configure the board LED for output. / 將板載 LED 設為輸出。
}

void loop()  // Run repeatedly while the sketch is active. / 草稿執行期間重複呼叫。
{
    digitalWrite(LED_BUILTIN, HIGH);  // Turn the built-in LED on. / 點亮板載 LED。
    delay(500);  // Hold the current LED state for half a second. / 維持目前 LED 狀態半秒。
    digitalWrite(LED_BUILTIN, LOW);  // Turn the built-in LED off. / 熄滅板載 LED。
    delay(500);  // Hold the current LED state for half a second. / 維持目前 LED 狀態半秒。
}
