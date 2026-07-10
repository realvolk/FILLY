use std::collections::HashMap;
use serde_json::Value;

type Callback = Box<dyn Fn(&str, &Value) + Send + Sync>;

pub struct Store {
    pub data: HashMap<String, Value>,
    subscribers: HashMap<String, Vec<Callback>>,
}

impl Store {
    pub fn new() -> Self {
        Self { data: HashMap::new(), subscribers: HashMap::new() }
    }

    pub fn get(&self, key: &str) -> Option<&Value> { self.data.get(key) }

    pub fn get_string(&self, key: &str) -> String {
        self.get(key).and_then(|v| v.as_str().map(|s| s.to_owned())).unwrap_or_default()
    }

    pub fn get_bool(&self, key: &str) -> bool {
        self.get(key).and_then(|v| v.as_bool()).unwrap_or(false)
    }

    pub fn set(&mut self, key: String, value: Value) {
        self.data.insert(key.clone(), value.clone());
        if let Some(callbacks) = self.subscribers.get(&key) {
            for cb in callbacks { cb(&key, &value); }
        }
        if let Some(callbacks) = self.subscribers.get("*") {
            for cb in callbacks { cb(&key, &value); }
        }
    }

    pub fn set_str(&mut self, key: String, value: &str) {
        self.set(key, Value::String(value.to_owned()));
    }

    pub fn subscribe<F>(&mut self, key: &str, callback: F)
    where F: Fn(&str, &Value) + Send + Sync + 'static
    {
        self.subscribers.entry(key.to_string()).or_insert_with(Vec::new).push(Box::new(callback));
    }
}

impl Clone for Store {
    fn clone(&self) -> Self {
        Self { data: self.data.clone(), subscribers: HashMap::new() }
    }
}