<?php

/**
 * @file
 * Default translation handler for the translation module.
 */


/**
 * Interface for enabling entity translation.
 *
 * Defines a set of methods to allow any entity to be processed by the entity
 * translation UI.
 */
interface EntityTranslationHandlerInterface {

  /**
   * Load the translation data into the wrapped entity.
   */
  public function loadTranslations();

  /**
   * Write the translation status to the storage.
   */
  public function saveTranslations();

  /**
   * Return the translation data for the current (wrapped) entity.
   */
  public function getTranslations();

  /**
   * Add/edit an entity translation.
   *
   * @param $translation
   *   A translation array as defined by the translation table's schema.
   * @param $values
   *   Optional. the values that should be assigned to the field translations.
   */
  public function setTranslation($translation, $values = NULL);

  /**
   * Remove a translation from the translation set.
   *
   * @param $langcode
   *   The language code of the translation to be removed.
   */
  public function removeTranslation($langcode);

  /**
   * Initialize the translation set by creating the original translation.
   */
  public function initTranslations();

  /**
   * Update the translation set from the current entity status.
   */
  public function updateTranslations();

  /**
   * Remove all translations from the translation set.
   */
  public function removeTranslations();

  /**
   * Initialize the language of the original field values.
   *
   * Ensure that the original translation language matches the language assigned
   * to the original field values.
   *
   * @return
   *   TRUE if any initialization was necessary, FALSE otherwise.
   */
  public function initOriginalTranslation();

  /**
   * Return the entity language.
   */
  public function getLanguage();

  /**
   * Set the language of the orginal translation.
   *
   * @param $langcode
   *   The language code of the original content values.
   */
  public function setOriginalLanguage($langcode);

  /**
   * Return TRUE if the entity is currently being translated.
   */
  public function isTranslating();

  /**
   * Notify the translation handler that its wrapped entity is being translated.
   *
   * @param $translating
   *   A boolean value.
   */
  public function setTranslating($translating);

  /**
   * Return TRUE if a new revision of the entity has just been created.
   */
  public function isRevision();

  /**
   * Replace the wrapped entity.
   *
   * @param $entity
   *   The entity to be translated.
   */
  public function setEntity($entity);

  /**
   * Set the translation update status.
   *
   * @param $outdated
   *   A boolean value.
   */
  public function setOutdated($outdated);

  /**
   * Return the base path for the current entity.
   *
   * This path will be prepended to the URL of any administration page.
   *
   * @return
   *   A string to be used as a URL path prefix.
   */
  public function getBasePath();

  /**
   * Return the path of the entity edit form.
   *
   * @param $langcode
   *   (optional) The language the edit form should be presented in.
   */
  public function getEditPath($langcode = NULL);

  /**
   * Return the path of the entity view page.
   */
  public function getViewPath();

  /**
   * A string allowing the user to identify the entity.
   */
  public function getLabel();

  /**
   * Check if the user can perform the given operation on the wrapped entity.
   *
   * @param $op
   *   The operation to be performed.
   *
   * @return
   *   TRUE if the user is allowed to perform the given operation, FALSE
   *   otherwise.
   */
  public function getAccess($op);

  /**
   * Return TRUE if the entity supports URL aliasing.
   */
  public function isAliasEnabled();

  /**
   * TODO
   */
  public function setFormLanguage($langcode);

  /**
   * TODO
   */
  public function getFormLanguage();

  /**
   * TODO
   */
  public function setSourceLanguage($langcode);

  /**
   * TODO
   */
  public function getSourceLanguage();

  /**
   * TODO
   */
  public function isNewEntity();

  /**
   * TODO
   */
  public function entityForm(&$form, &$form_state);

  /**
   * TODO
   */
  public function entityFormValidate($form, &$form_state);

  /**
   * TODO
   */
  public function entityFormSubmit($form, &$form_state);
}

/**
 * Class implementing the default entity translation behaviours.
 */
class EntityTranslationDefaultHandler implements EntityTranslationHandlerInterface {

  protected $entityType;
  protected $entity;
  protected $entityInfo;
  protected $entityId;

  private $translating;
  private $outdated;
  private $formLanguage;
  private $sourceLanguage;

  private $basePath;
  private $editPath;
  private $viewPath;

  public function __construct($entity_type, $entity_info, $entity, $entity_id) {
    $this->entityType = $entity_type;
    $this->entityInfo = $entity_info;
    $this->entity = $entity;
    $this->entityId = $entity_id;

    $this->translating = FALSE;
    $this->outdated = FALSE;
    $this->formLanguage = FALSE;
    $this->sourceLanguage = FALSE;

    $info = $entity_info['translation']['entity_translation'];
    $this->basePath = $this->getPathInstance($info['base path']);
    $this->editPath = isset($info['edit path']) ? $this->getPathInstance($info['edit path']) : FALSE;
    $this->viewPath = isset($info['view path']) ? $this->getPathInstance($info['view path']) : FALSE;
  }

  /**
   * Read the translation data from the storage.
   */
  public static function loadMultiple($entity_type, $entities) {
    $entity_info = entity_get_info($entity_type);
    $translations_key = $entity_info['entity keys']['translations'];

    foreach ($entities as $id => $entity) {
      $entities[$id]->{$translations_key} = EntityTranslationDefaultHandler::emptyTranslations();
    }

    $results = db_select('entity_translation', 'et')
      ->fields('et')
      ->condition('entity_type', $entity_type)
      ->condition('entity_id', array_keys($entities), 'IN')
      ->orderBy('entity_id')
      ->orderBy('created')
      ->execute();

    foreach ($results as $row) {
      $id = $row->entity_id;
      $entities[$id]->{$translations_key}->data[$row->language] = (array) $row;

      // Only the original translation has an empty source.
      if (empty($row->source)) {
        $entities[$id]->{$translations_key}->original = $row->language;
      }
    }
  }

  public function loadTranslations() {
    if (isset($this->entityId)) {
      $this->loadMultiple($this->entityType, array($this->entityId => $this->entity));
    }
    else {
      $this->entity->{$this->getTranslationsKey()} = $this->emptyTranslations();
    }
  }

  public function saveTranslations() {
    // Delete and insert, rather than update, in case a value was added.
    db_delete('entity_translation')
      ->condition('entity_type', $this->entityType)
      ->condition('entity_id', $this->entityId)
      ->execute();

    $translations = $this->getTranslations();

    if (count($translations->data)) {
      global $user;

      $columns = array('entity_type', 'entity_id', 'language', 'source', 'uid', 'status', 'translate', 'created', 'changed');
      $query = db_insert('entity_translation')->fields($columns);

      // These values should overridde the translation ones as they are not
      // supposed to change.
      $overrides = array(
        'entity_id' => $this->entityId,
        'entity_type' => $this->entityType,
      );

      // These instead are just defaults.
      $defaults = array(
        'source' => '',
        'uid' => $user->uid,
        'translate' => 0,
        'status' => 0,
        'created' => REQUEST_TIME,
        'changed' => REQUEST_TIME,
      );

      foreach ($translations->data as $langcode => $translation) {
        $translation = $overrides + $translation + $defaults;
        if ($this->outdated && $langcode != $translations->original) {
          $translation['translate'] = 1;
        }
        $query->values($translation);
      }

      $query->execute();
    }
  }

  public function getTranslations() {
    $translations_key = $this->getTranslationsKey();

    // Lazy load translations if for some reason the wrapped entity did not go
    // through hook_entity_load().
    if (!isset($this->entity->{$translations_key})) {
      $this->loadTranslations();
    }

    return $this->entity->{$translations_key};
  }

  public function setTranslation($translation, $values = NULL) {
    if (isset($translation['source']) && $translation['language'] == $translation['source']) {
      throw new Exception('Invalid translation language');
    }

    $translations = $this->getTranslations();
    $langcode = $translation['language'];

    $this->setTranslating(TRUE);

    if (isset($translations->data[$langcode])) {
      $translation = array_merge($translations->data[$langcode], $translation);
      $translation['changed'] = REQUEST_TIME;
    }

    $translations->data[$langcode] = $translation;

    if (is_array($values)) {
      // Update field translations.
      list(, , $bundle) = entity_extract_ids($this->entityType, $this->entity);
      foreach (field_info_instances($this->entityType, $bundle) as $instance) {
        $field_name = $instance['field_name'];
        $field = field_info_field($field_name);
        if ($field['translatable'] && isset($values[$field_name])) {
          $this->entity->{$field_name}[$langcode] = $values[$field_name][$langcode];
        }
      }
    }
  }

  public function removeTranslation($langcode) {
    $translations_key = $this->getTranslationsKey();

    if (!empty($langcode)) {
      unset($this->entity->{$translations_key}->data[$langcode]);
    }
    else {
      $this->entity->{$translations_key}->data = array();
    }

    list(, , $bundle) = entity_extract_ids($this->entityType, $this->entity);

    // Remove field translations.
    foreach (field_info_instances($this->entityType, $bundle) as $instance) {
      $field_name = $instance['field_name'];
      $field = field_info_field($field_name);

      if ($field['translatable']) {
        if (!empty($langcode)) {
          $this->entity->{$field_name}[$langcode] = array();
        }
        else {
          $this->entity->{$field_name} = array();
        }
      }
    }
  }

  public function initTranslations() {
    $langcode = $this->getLanguage();

    if (!empty($langcode)) {
      $translation = array('language' => $langcode, 'status' => (int) $this->getStatus());
      $this->setTranslation($translation);
      $this->setOriginalLanguage($langcode);
    }
  }

  public function updateTranslations() {
    $langcode = $this->getLanguage();

    // Only create a translation on edit if the translation set is empty:
    // the entity might have been created with language set to "language
    // neutral".
    if (empty($this->getTranslations()->data)) {
      $this->initTranslations();
    }
    elseif (!empty($langcode) && !$this->isTranslating()) {
      $this->setOriginalLanguage($langcode);
    }
  }

  public function removeTranslations() {
    $this->removeTranslation(NULL);
  }

  public function initOriginalTranslation() {
    $fixed = FALSE;
    $translations = $this->getTranslations();
    list(, , $bundle) = entity_extract_ids($this->entityType, $this->entity);

    foreach (field_info_instances($this->entityType, $bundle) as $instance) {
      $field_name = $instance['field_name'];
      $field = field_info_field($field_name);
      $langcode = count($this->entity->{$field_name}) == 1 ? key($this->entity->{$field_name}) : $translations->original;

      if ($langcode == LANGUAGE_NONE && $field['translatable']) {
        $this->entity->{$field_name}[$translations->original] = $this->entity->{$field_name}[$langcode];
        $this->entity->{$field_name}[$langcode] = array();
        $fixed = TRUE;
      }
    }

    return $fixed;
  }

  public function getLanguage() {
    if (!empty($this->entityInfo['entity keys']['language'])) {
      $language_key = $this->entityInfo['entity keys']['language'];
      if (!empty($this->entity->{$language_key})) {
        return $this->entity->{$language_key};
      }
    }
    return language_default()->language;
  }

  public function setLanguage($langcode) {
    $this->language = $langcode;
  }

  public function setOriginalLanguage($langcode) {
    $translations = $this->getTranslations();

    if (isset($translations->original) && $translations->original != $langcode) {
      $translations->data[$langcode] = $translations->data[$translations->original];
      $translations->data[$langcode]['language'] = $langcode;
      unset($translations->data[$translations->original]);
    }

    $translations->original = $langcode;
  }

  public function isTranslating() {
    return $this->translating;
  }

  public function setTranslating($translating) {
    $this->translating = $translating;
  }

  public function isRevision() {
    return FALSE;
  }

  public function setEntity($entity) {
    $this->entity = $entity;
  }

  public function setOutdated($outdated) {
    $this->outdated = $outdated;
  }

  public function getBasePath() {
    return $this->basePath;
  }

  public function getEditPath($langcode = NULL) {
    return empty($langcode) ? $this->editPath : $this->editPath . '/' . $langcode;
  }

  public function getViewPath() {
    return $this->viewPath;
  }

  public function getLabel() {
    if (($label = entity_label($this->entityType, $this->entity)) !== FALSE) {
      return $label;
    }
    else {
      return "{$this->entityType}:{$this->getEntityId()}" ;
    }
  }

  public function getAccess($op) {
    return TRUE;
  }

  public function isAliasEnabled() {
    return !empty($this->entityInfo['translation']['entity_translation']['alias']);
  }

  public function setFormLanguage($langcode) {
    $this->formLanguage = $langcode;
  }

  public function getFormLanguage() {
    return $this->formLanguage;
  }

  public function setSourceLanguage($langcode) {
    $this->sourceLanguage = $langcode;
  }

  public function getSourceLanguage() {
    return $this->sourceLanguage;
  }

  public function isNewEntity() {
    $id = $this->getEntityId();
    return empty($id);
  }

  public function entityForm(&$form, &$form_state) {
    $translations = $this->getTranslations();
    $form_langcode = $this->getFormLanguage();
    $langcode = $this->getLanguage();
    $is_translation = $langcode != $form_langcode;
    $new_translation = !isset($translations->data[$form_langcode]);

    // Display source language selector only if we are creating a new
    // translation and there are at least two translations available.
    if ($new_translation && count($translations->data) > 1) {
      $form['source_language'] = array(
        '#type' => 'fieldset',
        '#title' => t('Source language'),
        '#collapsible' => TRUE,
        '#collapsed' => TRUE,
        '#tree' => TRUE,
        '#weight' => -100,
        'language' => array(
          '#type' => 'select',
          '#default_value' => $this->getSourceLanguage(),
          '#options' => array(),
        ),
        'submit' => array(
          '#type' => 'submit',
          '#value' => t('Change'),
          '#submit' => array('entity_translation_entity_form_source_language_submit'),
        ),
      );
      foreach (language_list() as $language) {
        if (isset($translations->data[$language->language])) {
          $form['source_language']['language']['#options'][$language->language] = t($language->name);
        }
      }
    }

    if ($is_translation) {
      $languages = language_list();

      if (isset($form['language'])) {
        // If we are editing a translation set the correct value in the language
        // widget and not current one.
        // @todo Consider supporting the ability to change translation language.
        $form['language']['#options'][$langcode] = $languages[$langcode]->name;
        $form['language']['#default_value'] = $langcode;
        $form['language']['#disabled'] = TRUE;
      }

      // Replace the delete button with the delete translation one.
      if (!$new_translation) {
        $form['actions']['delete_translation'] = array(
          '#type' => 'submit',
          '#value' => t('Delete translation'),
          '#weight' => isset($form['actions']['delete']) ? $form['actions']['delete']['weight'] : 100,
          '#submit' => array('entity_translation_entity_form_delete_translation_submit'),
        );
      }

      // Always remove the delete button on translation forms.
      unset($form['actions']['delete']);

      // Set page title.
      $args = array('@label' => $this->getLabel(), '@language' => t($languages[$form_langcode]->name));
      drupal_set_title(t('@label [@language translation]', $args));
    }

    // We need to display the translation tab only when there is at least one
    // translation available or a new one is about to be created.
    if ($new_translation || count($translations->data) > 1) {
      $form['translation'] = array(
        '#type' => 'fieldset',
        '#title' => t('Translation'),
        '#collapsible' => TRUE,
        '#collapsed' => TRUE,
        '#tree' => TRUE,
        '#weight' => 10,
      );

      $status = $new_translation || $translations->data[$form_langcode]['status'];
      $enabled = !$status;
      // If there is only one published translation we cannot unpublish it,
      // since there would be no content left to display. The whole entity
      // should be unpublished instead, where possible.
      if (!empty($status)) {
        $published = 0;
        foreach ($translations->data as $langcode => $translation) {
          $published += $translation['status'];
        }
        $enabled = $published > 1;
      }
      $description = $enabled ?
        t('An unpublished translation will not be visible for non-administrators.') :
        t('Only this translation is published. You must publish at least one more translation to unpublish this one.');

      $form['translation']['status'] = array(
        '#type' => 'checkbox',
        '#title' => t('This translation is published'),
        '#default_value' => $status,
        '#description' => $description,
        '#disabled' => !$enabled,
      );

      if (!$is_translation) {
        $form['translation']['retranslate'] = array(
          '#type' => 'checkbox',
          '#title' => t('Flag translations as outdated'),
          '#default_value' => 0,
          '#description' => t('If you made a significant change, which means translations should be updated, you can flag all translations of this post as outdated. This will not change any other property of those posts, like whether they are published or not.'),
        );
      }
      else {
        $translate = !$new_translation && $translations->data[$form_langcode]['translate'];
        $form['translation']['translate'] = array(
          '#type' => 'checkbox',
          '#title' => t('This translation needs to be updated'),
          '#default_value' => $translate,
          '#description' => t('When this option is checked, this translation needs to be updated because the source post has changed. Uncheck when the translation is up to date again.'),
          '#disabled' => !$translate,
        );

        $name = $new_translation ? '' : user_load($translations->data[$form_langcode]['uid'])->name;
        $form['translation']['name'] = array(
          '#type' => 'textfield',
          '#title' => t('Authored by'),
          '#maxlength' => 60,
          '#autocomplete_path' => 'user/autocomplete',
          '#default_value' => $name,
          '#description' => t('Leave blank for %anonymous.', array('%anonymous' => variable_get('anonymous', t('Anonymous')))),
        );

        $date = $new_translation ? REQUEST_TIME : $translations->data[$form_langcode]['created'];
        $form['translation']['created'] = array(
          '#type' => 'textfield',
          '#title' => t('Authored on'),
          '#maxlength' => 25,
          '#description' => t('Format: %time. The date format is YYYY-MM-DD and %timezone is the time zone offset from UTC. Leave blank to use the time of form submission.', array('%time' => format_date($date, 'custom', 'Y-m-d H:i:s O'), '%timezone' => format_date($date, 'custom', 'O'))),
          '#default_value' => $new_translation ? '' : format_date($date, 'custom', 'Y-m-d H:i:s O'),
        );
      }
    }

    // We need to process the posted form as early as possible to update the
    // form language value.
    array_unshift($form['#validate'], 'entity_translation_entity_form_validate');

    // Process entity form submission. This allows to intercept deletions.
    $form['#submit'][] = 'entity_translation_entity_form_submit';
  }

  public function entityFormValidate($form, &$form_state) {}

  public function entityFormSubmit($form, &$form_state) {
    $langcode = $this->getLanguage();
    $form_langcode = $this->getFormLanguage();
    $translations = $this->getTranslations();
    $is_translation = $this->isTranslationForm();
    $new_translation = !isset($translations->data[$form_langcode]);
    $values = isset($form_state['values']['translation']) ? $form_state['values']['translation'] : array();

    // Ensure every key has at least a default value. Subclasses may provide use
    // entity-specific values to alter them.
    $values += array(
      'status' => TRUE,
      'retranslate' => 0,
      'name' => $GLOBALS['user']->name,
    );

    if (!isset($translations->data[$form_langcode])) {
      // If we have a new translation the language is the original entity
      // language.
      $translation = $is_translation ? array('language' => $form_langcode, 'source' => $this->getSourceLanguage()) : array('language' => $langcode, 'source' => '');
    }
    else {
      $translation = $translations->data[$form_langcode];
    }

    if ($is_translation) {
      $translation['translate'] = intval($values['translate']);
    }
    else {
      $this->setOriginalLanguage($langcode);
      $this->setOutdated($values['retranslate']);
    }

    $translation['status'] = intval($values['status']);
    $translation['uid'] = user_load_by_name($values['name'])->uid;
    $translation['created'] = empty($values['created']) ? REQUEST_TIME : strtotime($values['created']);
    $this->setTranslation($translation);

    // If no redirect has been explicitly set, go to the edit form for the
    // current form language.
    if ($new_translation && empty($form_state['redirect'])) {
      $form_state['redirect'] = $this->getEditPath($form_langcode);
    }
  }

  /**
   * Return TRUE if an entity translation is being edited.
   */
  protected function isTranslationForm() {
    return !$this->isNewEntity() && $this->getFormLanguage() != $this->getLanguage();
  }

  /**
   * Return the translation object key for the wrapped entity type.
   */
  protected function getTranslationsKey() {
    return $this->entityInfo['entity keys']['translations'];
  }

  /**
   * Return the entity accessibility.
   */
  protected function getStatus() {
    return TRUE;
  }

  /**
   * Return the entity identifier.
   */
  protected function getEntityId() {
    return $this->entityId;
  }

  /**
   * Return an instance of the given path.
   *
   * @param $path
   *   An internal path containing the entity id wildcard.
   *
   * @return
   *   The instantiated path.
   */
  protected function getPathInstance($path) {
    $wildcard = $this->entityInfo['translation']['entity_translation']['path wildcard'];
    return str_replace($wildcard, $this->getEntityId(), $path);
  }

  /**
   * Return an empty translations data structure.
   */
  protected static function emptyTranslations() {
    return (object) array('original' => NULL, 'data' => array());
  }
}
