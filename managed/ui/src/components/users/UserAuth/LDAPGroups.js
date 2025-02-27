import React from 'react';
import * as Yup from 'yup';
import { Box, MenuItem, IconButton, makeStyles } from '@material-ui/core';
import { useFieldArray, useForm } from 'react-hook-form';
import { yupResolver } from '@hookform/resolvers/yup';
import { YBModal, YBButton, YBInputField, YBSelectField } from '../../../redesign/components';
import { YBA_ROLES } from './LDAPAuth';
import TrashIcon from '../icons/trash_icon';
import RemoveIcon from '../icons/remove_icon';
import AddIcon from '../icons/add_icon';

const useStyles = makeStyles((theme) => ({
  deleteBtn: {
    height: 40
  },
  mapTable: {
    background: theme.palette.ybacolors.ybBackgroundGray,
    border: `1px solid #CCCCCC`,
    borderRadius: theme.spacing(1),
    padding: theme.spacing(3)
  },
  addRowBtn: {
    padding: 0
  }
}));

//validation
Yup.addMethod(Yup.array, 'unique', function (field, message) {
  return this.test('unique', message, function (array) {
    const uniqueData = Array.from(new Set(array.map((row) => row[field]?.toLowerCase())));
    const isUnique = array.length === uniqueData.length;
    if (isUnique) {
      return true;
    }
    const index = array.findIndex((row, i) => row[field]?.toLowerCase() !== uniqueData[i]);
    if (array[index][field] === '') {
      return true;
    }
    return this.createError({
      path: `${this.path}.${index}.${field}`,
      message
    });
  });
});

const schema = Yup.object().shape({
  mapping: Yup.array()
    .unique('distinguishedName', 'Group DN must be unique.')
    .of(
      Yup.object().shape({
        ybaRole: Yup.string().required('Role is required.'),
        distinguishedName: Yup.string().required('Group DN is required')
      })
    )
});
//validation

const DEFAULT_MAPPING_VALUE = [
  {
    ybaRole: 'Admin',
    distinguishedName: ''
  },
  {
    ybaRole: 'BackupAdmin',
    distinguishedName: ''
  },
  {
    ybaRole: 'ReadOnly',
    distinguishedName: ''
  }
];

export const LDAPMappingModal = ({ open, onClose, onSubmit, values }) => {
  const classes = useStyles();

  const {
    control,
    getValues,
    setValue,
    trigger,
    formState: { isValid }
  } = useForm({
    mode: 'onChange',
    defaultValues: {
      mapping: values && values.length ? values : DEFAULT_MAPPING_VALUE
    },
    resolver: yupResolver(schema)
  });
  const { fields, append, remove } = useFieldArray({
    control,
    name: 'mapping'
  });

  const handleFormSubmit = () => {
    if (!isValid) trigger();
    else onSubmit(getValues('mapping'));
  };

  return (
    <YBModal
      open={open}
      overrideWidth={1110}
      overrideHeight="auto"
      titleSeparator
      cancelLabel="Cancel"
      submitLabel="Confirm"
      size="lg"
      title="Create Mapping"
      onClose={onClose}
      onSubmit={handleFormSubmit}
      dialogContentProps={{ style: { paddingTop: 20 } }}
      submitTestId="submit-ldap-groups"
      cancelTestId="close-ldap-groups"
    >
      <Box
        display="flex"
        width="100%"
        flexDirection="column"
        data-testid="ldap-groups-modal"
        mb={2}
      >
        <Box mt={0.5}>
          Add LDAP Groups you want to map to the following role. Make sure to add them one
          at a time.
        </Box>
        <Box mt={1} display="flex" justifyContent="flex-end">
          <YBButton
            variant="secondary"
            className={classes.deleteBtn}
            disabled={!values.length}
            onClick={() => {
              setValue('mapping', []);
            }}
          >
            <TrashIcon /> &nbsp;
            <>Delete All Mappings </>
          </YBButton>
        </Box>

        <Box mt={1} display="flex" className={classes.mapTable} flexDirection="column">
          {fields.length > 0 && (
            <Box display="flex" mb={0.5}>
              <Box display="flex" width={160}>
                Role
              </Box>
              <Box ml={1} display="flex" width="100%">
                Group DN
              </Box>
              <Box display="flex" width={24} mx={1}></Box>
            </Box>
          )}
          <Box>
            {fields.map((field, index) => {
              return (
                <Box display="flex" alignItems="center" mb={1} key={field.id}>
                  <Box display="flex" width={160}>
                    <YBSelectField fullWidth name={`mapping.${index}.ybaRole`} control={control}>
                      {YBA_ROLES.map((role) => (
                        <MenuItem key={role.value} value={role.value}>
                          {role.label}
                        </MenuItem>
                      ))}
                    </YBSelectField>
                  </Box>
                  <Box ml={1} display="flex" width="100%">
                    <YBInputField
                      fullWidth
                      name={`mapping.${index}.distinguishedName`}
                      control={control}
                      placeholder="Example:  cn = eng , ou = backend, dc = Yugabyte, dc = com"
                    />
                  </Box>
                  <Box display="flex" width={24} mx={1}>
                    <IconButton onClick={() => remove(index)}>
                      <RemoveIcon />
                    </IconButton>
                  </Box>
                </Box>
              );
            })}
          </Box>
          <Box mt={1}>
            <YBButton
              variant="ghost"
              onClick={() => append({ ybaRole: '', distinguishedName: '' }, { shouldFocus: false })}
              className={classes.addRowBtn}
            >
              <AddIcon /> &nbsp;
              <>Add rows </>
            </YBButton>
          </Box>
        </Box>
      </Box>
    </YBModal>
  );
};
