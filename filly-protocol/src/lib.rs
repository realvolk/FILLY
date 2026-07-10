use serde::{Deserialize, Serialize};
use serde_json::Value;

#[derive(Debug, Deserialize)]
pub struct WidgetRequest {
    pub widget: String,
    #[serde(default)]
    pub step: Option<u16>,
    #[serde(default)]
    pub total: Option<u16>,
    #[serde(default)]
    pub params: Value,
    #[serde(default)]
    pub session_id: Option<String>,
}

#[derive(Debug, Serialize)]
pub struct WidgetResponse {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub result: Option<Value>,
    #[serde(default)]
    pub cancelled: bool,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub error: Option<String>,
}

#[derive(Debug, Deserialize)]
#[serde(untagged)]
pub enum Message {
    Request(WidgetRequest),
    Batch(Vec<WidgetRequest>),
}